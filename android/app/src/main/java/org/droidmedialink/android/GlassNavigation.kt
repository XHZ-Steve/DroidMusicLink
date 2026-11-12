package org.droidmedialink.android

import androidx.compose.animation.core.*
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.*
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.kyant.backdrop.backdrops.LayerBackdrop
import com.kyant.backdrop.drawBackdrop
import com.kyant.backdrop.effects.blur
import com.kyant.backdrop.effects.lens

@Composable
fun GlassNavigation(page:Int,onPage:(Int)->Unit,backdrop:LayerBackdrop,glass:Boolean,modifier:Modifier=Modifier) {
 val colors=MaterialTheme.colorScheme
 val stretch=remember { Animatable(1f) }
 LaunchedEffect(page) {
  stretch.animateTo(1.08f,tween(90))
  stretch.animateTo(1f,spring(dampingRatio=.7f,stiffness=420f))
 }
 BoxWithConstraints(modifier.navigationBarsPadding().padding(horizontal=20.dp,vertical=10.dp).fillMaxWidth().height(80.dp)) {
  val segment=maxWidth/3
  val lensWidth=72.dp
  val lensHeight=40.dp
  val offset by animateDpAsState(segment*page+(segment-lensWidth)/2,spring(dampingRatio=.68f,stiffness=420f),label="navigationLens")
  Box(Modifier.matchParentSize().then(if(glass)Modifier.drawBackdrop(backdrop=backdrop,shape={RoundedCornerShape(40.dp)},effects={blur(2.dp.toPx());lens(12.dp.toPx(),24.dp.toPx())},onDrawSurface={drawRect(colors.surface.copy(alpha=.18f))}) else Modifier.background(colors.surfaceContainer,RoundedCornerShape(40.dp))))
  Box(Modifier.offset(x=offset,y=10.dp).size(lensWidth,lensHeight).graphicsLayer { scaleX=stretch.value;scaleY=1f-(stretch.value-1f)*.25f }.then(if(glass)Modifier.drawBackdrop(backdrop=backdrop,shape={RoundedCornerShape(18.dp)},effects={blur(.7.dp.toPx());lens(7.dp.toPx(),12.dp.toPx())},onDrawSurface={drawRect(Color.White.copy(alpha=.035f))}) else Modifier.background(colors.onSurface.copy(alpha=.09f),RoundedCornerShape(18.dp))))
  Row(Modifier.fillMaxSize()) {
   listOf(R.string.home to Lucide.Home,R.string.devices to Lucide.Computer,R.string.settings to Lucide.Settings).forEachIndexed { i,(label,icon) ->
    NavigationBarItem(selected=page==i,onClick={onPage(i)},icon={LucideIcon(icon,stringResource(label))},label={Text(stringResource(label),maxLines=1)},colors=NavigationBarItemDefaults.colors(indicatorColor=Color.Transparent,selectedIconColor=colors.onSurface,selectedTextColor=colors.onSurface,unselectedIconColor=colors.onSurfaceVariant,unselectedTextColor=colors.onSurfaceVariant))
   }
  }
 }
}
