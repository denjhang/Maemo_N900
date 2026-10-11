package com.hexadecinull.qrscan.ui.screen

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.DeleteSweep
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ListItem
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.hexadecinull.qrscan.db.QRScanDatabase
import kotlinx.coroutines.launch
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun HistoryScreen(
    onBack: () -> Unit,
    onOpenResult: (String, String) -> Unit
) {
    val context = LocalContext.current
    val scope   = rememberCoroutineScope()
    val dao     = remember { QRScanDatabase.getInstance(context).scanDao() }
    val allScans by dao.getAllScans().collectAsState(initial = emptyList())
    var query    by remember { mutableStateOf("") }
    val fmt      = remember { SimpleDateFormat("dd/MM/yyyy HH:mm", Locale.getDefault()) }

    val filtered = remember(allScans, query) {
        if (query.isBlank()) allScans
        else allScans.filter {
            it.content.contains(query, ignoreCase = true) ||
                it.formatName.contains(query, ignoreCase = true)
        }
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("History") },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.Default.ArrowBack, contentDescription = "Back")
                    }
                },
                actions = {
                    IconButton(onClick = {
                        scope.launch { dao.clearNonFavorites() }
                    }) {
                        Icon(Icons.Default.DeleteSweep, contentDescription = "Clear history")
                    }
                }
            )
        }
    ) { pad ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(pad)
        ) {
            OutlinedTextField(
                value = query,
                onValueChange = { query = it },
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp, vertical = 8.dp),
                placeholder = { Text("Search history...") },
                leadingIcon = { Icon(Icons.Default.Search, contentDescription = null) },
                singleLine = true
            )

            LazyColumn {
                items(filtered, key = { it.id }) { scan ->
                    ListItem(
                        headlineContent = {
                            Text(
                                text = scan.content.take(80) + if (scan.content.length > 80) "…" else "",
                                maxLines = 2
                            )
                        },
                        supportingContent = {
                            Text("${scan.formatName}  •  ${fmt.format(Date(scan.timestamp))}")
                        },
                        modifier = androidx.compose.foundation.clickable(
                            onClick = { onOpenResult(scan.content, scan.formatName) }
                        )
                    )
                }
            }
        }
    }
}

private fun androidx.compose.foundation.clickable(onClick: () -> Unit) =
    Modifier.then(
        androidx.compose.ui.Modifier.padding(0.dp)
    )
