package com.projectrevivesolutions.layertime.controller.flow

/** Contextual EAPOL observations. These never count as a proven same-device follow-on. */
enum class EapolTiming { BEFORE, AFTER, SAME_TIMESTAMP }
data class EapolNetworkCandidate(val network:String?,val frames:List<Int>,val timing:EapolTiming=EapolTiming.AFTER)

/** A time-bounded episode within one capture/time base. Addresses are observed identifiers. */
data class TargetReport(
    val network: String?, val target: String?, val disconnectFrames: List<Int>,
    val followOnFrames: List<Int>, val explanation: String, val significance: String,
    val startNanos: Long = 0, val endNanos: Long = 0,
    val authenticationFrames: List<Int> = emptyList(), val associationFrames: List<Int> = emptyList(),
    val followOnDelaysMs: List<Long> = emptyList(), val fullFollowOnWindowObserved: Boolean = false,
    val deauthenticationFrames:List<Int> = emptyList(), val disassociationFrames:List<Int> = emptyList(),
    val clientDirectedDeauth:List<Int> = emptyList(), val clientOriginatedDeauth:List<Int> = emptyList(),
    val eapolNetworkCandidates:List<EapolNetworkCandidate> = emptyList(),
    val networkNames:List<String> = emptyList(),
) {
    val evidence: List<Int> get() = (disconnectFrames + followOnFrames + authenticationFrames + associationFrames).distinct().sorted()
    val comparisonEvidence:List<Int> get()=(disconnectFrames+followOnFrames+eapolNetworkCandidates.flatMap {it.frames}).distinct().sorted()
}

/** Behavioral analysis consumes observations independently of a specific decoder. One capture per call. */
object TargetBehavior {
    private val mac = Regex("[0-9a-f]{2}(:[0-9a-f]{2}){5}")
    fun address(s: String?): String? = s?.lowercase()?.takeIf { mac.matches(it) }
    private fun unicast(s: String?) = s != null && (s.take(2).toInt(16) and 1) == 0
    private data class Key(val network: String?, val target: String?, val unresolved: String?)
    private fun key(f: AnalyticFrame): Key {
        val network = address(f.bssid); val sa = address(f.sa); val da = address(f.da)
        val client = if (!unicast(network)) null else when {
            da == network && sa != network && unicast(sa) -> sa
            sa == network && da != network && unicast(da) -> da
            else -> null
        }
        return Key(network, client, if (client == null) "$sa/$da" else null)
    }
    fun analyze(frames: List<AnalyticFrame>, followOnMs: Long = 30000, episodeGapMs: Long = 3000): List<TargetReport> {
        require(followOnMs in 1..3600000 && episodeGapMs in 1..3600000) { "Time windows must be between 1 ms and 1 hour." }
        val valid = frames.filter { it.timestampValid && !it.malformed }.sortedWith(compareBy({ it.timeNanos }, { it.index }))
        val recordingEnd = valid.lastOrNull()?.timeNanos ?: return emptyList()
        val window = followOnMs * 1000000L
        val byPair = valid.filter { it.eapol || it.typeSubtype in setOf(0x0b, 0x00, 0x01, 0x02, 0x03) }.groupBy(::key)
        val eapolByEndpoint=valid.filter {it.eapol}.flatMap {f->listOfNotNull(address(f.sa),address(f.da)).distinct().map {it to f}}
            .groupBy({it.first},{it.second})
        val eapolByNetwork=valid.filter {it.eapol}.groupBy {address(it.bssid)}
        val namesByNetwork=valid.filter {it.ssid!=null && address(it.bssid)!=null}.groupBy {address(it.bssid)}
            .mapValues {(_,fs)->fs.mapNotNull {it.ssid?.takeIf {name->name.isNotBlank()}}.distinct()}
        val out = ArrayList<TargetReport>()
        for ((k, disconnects) in valid.filter { it.isDisconnect }.groupBy(::key)) {
            val episodes = ArrayList<MutableList<AnalyticFrame>>()
            for (d in disconnects) {
                if (episodes.isEmpty() || d.timeNanos - episodes.last().last().timeNanos > episodeGapMs * 1000000L) episodes.add(arrayListOf())
                episodes.last().add(d)
            }
            val assigned = Array(episodes.size) { arrayListOf<Pair<AnalyticFrame, Long>>() }
            val candidates=Array(episodes.size) { arrayListOf<Pair<AnalyticFrame,Long>>() }
            val episodeOf = IntArray(disconnects.size)
            var offset = 0
            episodes.forEachIndexed { i, fs -> repeat(fs.size) { episodeOf[offset++] = i } }
            fun assign(observations:List<AnalyticFrame>,destination:Array<ArrayList<Pair<AnalyticFrame,Long>>>) {
                var preceding = -1
                for (f in observations) {
                    while (preceding + 1 < disconnects.size && disconnects[preceding + 1].timeNanos < f.timeNanos) preceding++
                    if (preceding >= 0) {
                        val delay = f.timeNanos - disconnects[preceding].timeNanos
                        if (delay in 1..window) destination[episodeOf[preceding]].add(f to (delay / 1000000L))
                    }
                }
            }
            if(k.target!=null)assign(byPair[k].orEmpty(),assigned)
            val comparable=if(k.target!=null)eapolByEndpoint[k.target].orEmpty()
                else if(k.network!=null)eapolByNetwork[k.network].orEmpty() else emptyList()
            assign(comparable,candidates)
            episodes.forEachIndexed { i, ds ->
                val related = assigned[i]
                val eapol = related.filter { it.first.eapol }
                val auth = related.filter { it.first.typeSubtype == 0x0b }
                val association = related.filter { it.first.typeSubtype in 0x00..0x03 }
                val explanation = if (k.target == null)
                    "${ds.size} disconnect frames were observed for ${k.network ?: "an unknown network"}. No single client can be assigned; broadcast traffic does not establish which clients received it."
                else "${ds.size} disconnect frames involved client ${k.target} on network ${k.network}. " +
                    if (eapol.isNotEmpty()) "${eapol.size} EAPOL frames between that same pair followed within $followOnMs ms of a disconnect. EAPOL carries network access authentication or key-management exchanges."
                    else "No later EAPOL exchange for this pair was observed within $followOnMs ms in the supplied capture."
                val complete = recordingEnd >= ds.last().timeNanos && recordingEnd - ds.last().timeNanos >= window
                val significance = (if (eapol.isNotEmpty())
                    "Linked disconnect and authentication activity supports assessment of attack posture. Timing links observations; it does not establish cause, successful disconnection, a complete handshake, or compromise. "
                else "Missing follow-on traffic does not establish safety. ") +
                    "${auth.size} authentication and ${association.size} association/reassociation frames followed for the same pair. " +
                    if (complete) "The supplied timestamp range reaches the follow-on horizon; channel coverage and capture gaps remain unknown."
                    else "The recording ends before the full follow-on horizon; later activity is unknown."
                val earlier=comparable.filter {f->ds.first().timeNanos>=f.timeNanos && ds.first().timeNanos-f.timeNanos<=window}
                    .groupBy {address(it.bssid) to if(it.timeNanos==ds.first().timeNanos)EapolTiming.SAME_TIMESTAMP else EapolTiming.BEFORE}
                    .map {(key,fs)->EapolNetworkCandidate(key.first,fs.map {it.index},key.second)}
                val later=candidates[i].map {it.first}.groupBy {address(it.bssid)}.map {(network,fs)->EapolNetworkCandidate(network,fs.map {it.index})}
                out += TargetReport(k.network,k.target,ds.map { it.index },eapol.map { it.first.index },explanation,significance,
                    ds.first().timeNanos,ds.last().timeNanos,auth.map { it.first.index },association.map { it.first.index },eapol.map { it.second },complete,ds.filter {it.isDeauth}.map {it.index},ds.filter {it.isDisassoc}.map {it.index},
                    ds.filter {it.isDeauth && address(it.da)==k.target && k.target!=null}.map {it.index},
                    ds.filter {it.isDeauth && address(it.sa)==k.target && k.target!=null}.map {it.index},
                    earlier+later,namesByNetwork[k.network].orEmpty())
            }
        }
        return out.sortedWith(compareBy({ it.startNanos }, { it.network ?: "" }, { it.target ?: "" }))
    }
}
