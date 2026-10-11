package com.hexadecinull.qrscan.ui.screen

import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.Favorite
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ListItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import com.hexadecinull.qrscan.db.QRScanDatabase

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun FavoritesScreen(
    onBack: () -> Unit,
    onOpenResult: (String, String) -> Unit
) {
    val context  = LocalContext.current
    val dao      = remember { QRScanDatabase.getInstance(context).scanDao() }
    val favorites by dao.getFavorites().collectAsState(initial = emptyList())

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Favorites") },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.Default.ArrowBack, contentDescription = "Back")
                    }
                }
            )
        }
    ) { pad ->
        LazyColumn(modifier = Modifier.fillMaxSize().padding(pad)) {
            if (favorites.isEmpty()) {
                item {
                    androidx.compose.foundation.layout.Box(
                        modifier = Modifier.fillParentMaxSize(),
                        contentAlignment = androidx.compose.ui.Alignment.Center
                    ) {
                        androidx.compose.foundation.layout.Column(
                            horizontalAlignment = androidx.compose.ui.Alignment.CenterHorizontally
                        ) {
                            Icon(
                                Icons.Default.Favorite,
                                contentDescription = null,
                                modifier = androidx.compose.ui.Modifier.then(
                                    androidx.compose.ui.Modifier.padding(bottom = androidx.compose.foundation.layout.PaddingValues(16.dp).calculateBottomPadding())
                                )
                            )
                            Text("No favorites yet")
                        }
                    }
                }
            }
            items(favorites, key = { it.id }) { scan ->
                ListItem(
                    headlineContent = {
                        Text(scan.content.take(80) + if (scan.content.length > 80) "…" else "")
                    },
                    supportingContent = { Text(scan.formatName) },
                    modifier = androidx.compose.ui.Modifier.then(
                        androidx.compose.ui.Modifier.padding(androidx.compose.foundation.layout.PaddingValues())
                    )
                )
            }
        }
    }
}
