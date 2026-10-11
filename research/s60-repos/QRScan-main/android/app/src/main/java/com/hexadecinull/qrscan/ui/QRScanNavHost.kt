package com.hexadecinull.qrscan.ui

import android.net.Uri
import androidx.compose.animation.AnimatedContentTransitionScope
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.runtime.Composable
import androidx.navigation.NavHostController
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import com.hexadecinull.qrscan.ui.screen.CreateScreen
import com.hexadecinull.qrscan.ui.screen.FavoritesScreen
import com.hexadecinull.qrscan.ui.screen.HistoryScreen
import com.hexadecinull.qrscan.ui.screen.ResultScreen
import com.hexadecinull.qrscan.ui.screen.ScannerScreen
import com.hexadecinull.qrscan.ui.screen.SettingsScreen
import java.net.URLDecoder
import java.net.URLEncoder

object Routes {
    const val SCANNER  = "scanner"
    const val RESULT   = "result/{encodedContent}/{formatName}"
    const val HISTORY  = "history"
    const val FAVORITES = "favorites"
    const val CREATE   = "create"
    const val SETTINGS = "settings"

    fun result(content: String, formatName: String): String {
        val enc = URLEncoder.encode(content, "UTF-8")
        return "result/$enc/$formatName"
    }
}

@Composable
fun QRScanNavHost(
    navController: NavHostController = rememberNavController(),
    sharedImageUri: Uri? = null
) {
    NavHost(
        navController = navController,
        startDestination = Routes.SCANNER,
        enterTransition = {
            slideIntoContainer(AnimatedContentTransitionScope.SlideDirection.Start, tween(280)) +
                fadeIn(tween(280))
        },
        exitTransition = {
            slideOutOfContainer(AnimatedContentTransitionScope.SlideDirection.Start, tween(280)) +
                fadeOut(tween(280))
        },
        popEnterTransition = {
            slideIntoContainer(AnimatedContentTransitionScope.SlideDirection.End, tween(280)) +
                fadeIn(tween(280))
        },
        popExitTransition = {
            slideOutOfContainer(AnimatedContentTransitionScope.SlideDirection.End, tween(280)) +
                fadeOut(tween(280))
        }
    ) {
        composable(Routes.SCANNER) {
            ScannerScreen(
                sharedImageUri = sharedImageUri,
                onResult = { content, format ->
                    navController.navigate(Routes.result(content, format))
                },
                onHistory   = { navController.navigate(Routes.HISTORY) },
                onFavorites = { navController.navigate(Routes.FAVORITES) },
                onCreate    = { navController.navigate(Routes.CREATE) },
                onSettings  = { navController.navigate(Routes.SETTINGS) }
            )
        }

        composable(
            route = Routes.RESULT,
            arguments = listOf(
                navArgument("encodedContent") { type = NavType.StringType },
                navArgument("formatName")     { type = NavType.StringType }
            )
        ) { backStack ->
            val content = URLDecoder.decode(
                backStack.arguments?.getString("encodedContent") ?: "", "UTF-8"
            )
            val format = backStack.arguments?.getString("formatName") ?: ""
            ResultScreen(
                content    = content,
                formatName = format,
                onBack     = { navController.popBackStack() },
                onCreate   = { navController.navigate(Routes.CREATE) }
            )
        }

        composable(Routes.HISTORY) {
            HistoryScreen(
                onBack = { navController.popBackStack() },
                onOpenResult = { content, format ->
                    navController.navigate(Routes.result(content, format))
                }
            )
        }

        composable(Routes.FAVORITES) {
            FavoritesScreen(
                onBack = { navController.popBackStack() },
                onOpenResult = { content, format ->
                    navController.navigate(Routes.result(content, format))
                }
            )
        }

        composable(Routes.CREATE) {
            CreateScreen(onBack = { navController.popBackStack() })
        }

        composable(Routes.SETTINGS) {
            SettingsScreen(onBack = { navController.popBackStack() })
        }
    }
}
