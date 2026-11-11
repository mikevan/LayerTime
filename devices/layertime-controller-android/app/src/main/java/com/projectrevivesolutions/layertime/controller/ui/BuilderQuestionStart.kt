package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.projectrevivesolutions.layertime.controller.ui.theme.LtColors

internal enum class BuilderPurpose { USE, BUILD, PRACTICE }

@Composable
internal fun BuilderQuestionStart(onChoose:()->Unit,onPractice:()->Unit,onCustom:()->Unit) {
    OpsConnectionHousing(Modifier.fillMaxWidth()) {
        Column(Modifier.fillMaxWidth().padding(18.dp),verticalArrangement=Arrangement.spacedBy(12.dp)) {
            Text("WHAT WOULD YOU LIKE TO DO?",color=LtColors.Cyan,fontSize=18.sp)
            Text("Use an analysis",color=LtColors.Ivory,fontSize=21.sp)
            Text("Choose a ready-made analysis, load a recording, and read the report.",color=LtColors.Grey,fontSize=15.sp)
            SciFiPrimary("USE AN ANALYSIS",true,large=true,onClick=onChoose)
            Text("Build an analysis",color=LtColors.Ivory,fontSize=19.sp)
            Text("Combine tools and write your own report.",color=LtColors.Grey,fontSize=15.sp)
            SciFiOutline("BUILD AN ANALYSIS",true,large=true,onClick=onCustom)
            Text("Practice",color=LtColors.Ivory,fontSize=19.sp)
            Text("Try a generated recording with a known result. No Hunter is needed.",color=LtColors.Grey,fontSize=15.sp)
            SciFiOutline("TRY THE PRACTICE EXAMPLE",true,large=true,onClick=onPractice)
        }
    }
}
