package com.projectrevivesolutions.layertime.controller.flow

/** Short reading layer. Packet names remain explicit and technical detail remains inspectable. */
data class DeviceReading(val device:String?,val network:String?,val reports:List<TargetReport>) {
    val networkComparison get()=NetworkComparisons.read(reports)
    val networkNames get()=reports.flatMap {it.networkNames}.distinct()
    val deauth get()=reports.flatMap {it.deauthenticationFrames}.distinct()
    val directed get()=reports.flatMap {it.clientDirectedDeauth}.distinct()
    val originated get()=reports.flatMap {it.clientOriginatedDeauth}.distinct()
    val setup get()=reports.flatMap {it.followOnFrames}.distinct()
    val disassociation get()=reports.flatMap {it.disassociationFrames}.distinct()
    val observation:String get()=buildString {
        if(device==null)append("${deauth.size} deauthentication (deauth) packets were observed. A particular target device could not be identified. ")
        else {
            append("Device with MAC address $device ")
            if(directed.isNotEmpty())append("was the target of ${directed.size} deauthentication (deauth) packets")
            else append("was involved in ${deauth.size} deauthentication (deauth) packets")
            append(" across ${reports.size} burst(s). These messages ask a device to leave the network. ")
            if(originated.isNotEmpty())append("${originated.size} deauth packets were sent by this device. ")
        }
        if(setup.isNotEmpty())append("Afterward, ${setup.size} EAPOL messages for the same device and network were recorded. These messages are used to set up network access or encryption and can occur during reconnection.")
        else append("No later connection-setup messages for the same device and network were recorded.")
    }
    val significance:String get()=if(setup.isNotEmpty())
        "This pattern may indicate a forced reconnection. On some password-protected Wi-Fi networks, recorded key-exchange data can allow offline guessing of a weak password. Repeated deauth requests can also be used to disrupt network access (denial of service). This recording does not establish that anyone else captured a usable exchange or recovered a password."
    else "Repeated deauth requests can be used to disrupt network access (denial of service). The recording does not show what happened afterward."
    val briefReason:String get()=if(setup.isNotEmpty())
        "A device received requests to disconnect, followed by connection-setup messages for the same device and Wi-Fi access point. This may indicate a forced reconnection or disruption."
    else "Requests to disconnect were recorded. Review whether they are repeating or disrupting access."
    val action:String get()=if(setup.isNotEmpty())"Increase your threat posture and review this activity." else "Review the disconnect requests and watch for repeated disruption."
}

object ReportReading {
    fun devices(reports:List<TargetReport>):List<DeviceReading> = reports.groupBy {it.network to it.target}
        .map { (key,rs)->DeviceReading(key.second,key.first,rs) }

    fun observed(report:TargetReport):String=buildString {
        if(report.deauthenticationFrames.isNotEmpty()) append("${report.deauthenticationFrames.size} deauthentication (deauth) packet(s) asked this device to leave the network. ")
        if(report.disassociationFrames.isNotEmpty()) append("${report.disassociationFrames.size} disassociation packet(s) asked this device to end its connection. ")
        if(report.followOnFrames.isNotEmpty()) append("Afterward, ${report.followOnFrames.size} EAPOL packet(s) were seen for the same device and network. These are messages used to set up network access or encryption.")
        else append("No later EAPOL connection-setup messages for this device and network were seen in this recording.")
    }
    fun significance(report:TargetReport):String=if(report.followOnFrames.isNotEmpty())
        "A disconnect request followed by connection setup is a pattern worth examining for a forced reconnection. The packets show the requests and later messages; they do not show whether the device obeyed the request."
    else "A disconnect request can happen during normal network use or deliberate disruption. This recording does not show what happened next."
}
