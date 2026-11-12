package org.droidmedialink.android
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.*
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.*
import androidx.compose.ui.semantics.*
import androidx.compose.ui.unit.*
object Lucide {
 const val Home:Int=57589
 const val Computer:Int=57629
 const val Settings:Int=57684
 const val Wifi:Int=57774
 const val Notifications:Int=57433
 const val LinkOff:Int=57756
 const val Search:Int=57681
 const val HelpOutline:Int=57474
 const val MusicNote:Int=57634
}
@Composable fun LucideIcon(code:Int,contentDescription:String?,modifier:Modifier=Modifier,tint:Color=LocalContentColor.current) {
 BoxWithConstraints(modifier.size(24.dp).clearAndSetSemantics { if(contentDescription!=null)this.contentDescription=contentDescription },contentAlignment=Alignment.Center) {
  Text(code.toChar().toString(),fontFamily=FontFamily(Font(R.font.lucide)),fontSize=maxHeight.value.sp,lineHeight=maxHeight.value.sp,color=tint)
 }
}
