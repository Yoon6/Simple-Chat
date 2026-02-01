package com.yoon6.simplesocket

import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.util.Log
import android.view.View
import android.widget.Button
import android.widget.EditText
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.*
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.PrintWriter
import java.net.Socket

class MainActivity : AppCompatActivity() {

    private lateinit var adapter: ChatAdapter
    private val messages = mutableListOf<ChatMessage>()
    
    // Networking
    private var socket: Socket? = null
    private var writer: PrintWriter? = null
    private var reader: BufferedReader? = null
    
    // Coroutine Scope
    private val scope = CoroutineScope(Dispatchers.IO + Job())

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val rootView = findViewById<View>(R.id.rootView)
        ViewCompat.setOnApplyWindowInsetsListener(rootView) { v, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())

            // 상태바와 내비게이션 바 높이만큼 Padding 설정
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, 0)

            insets
        }
        val inputContainer = findViewById<View>(R.id.inputContainer)
        ViewCompat.setOnApplyWindowInsetsListener(inputContainer) { v, insets ->
            val combinedInsets = insets.getInsets(
                WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.ime()
            )
            // 상태바와 내비게이션 바 높이만큼 Padding 설정
            v.setPadding(v.paddingLeft, v.paddingTop, v.paddingRight, combinedInsets.bottom)

            insets
        }


        // Setup RecyclerView
        val recyclerView = findViewById<RecyclerView>(R.id.recyclerView)
        adapter = ChatAdapter(messages)
        recyclerView.adapter = adapter
        recyclerView.layoutManager = LinearLayoutManager(this)

        val editMessage = findViewById<EditText>(R.id.editMessage)
        val btnSend = findViewById<Button>(R.id.btnSend)

        // 1. TextWatcher for Button Visibility
        editMessage.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {
                if (s.isNullOrEmpty()) {
                    btnSend.visibility = View.GONE
                } else {
                    btnSend.visibility = View.VISIBLE
                }
            }
            override fun afterTextChanged(s: Editable?) {}
        })

        // Connect to server
        connectToServer()

        // Send Button Logic
        btnSend.setOnClickListener {
            val msg = editMessage.text.toString()
            if (msg.isNotEmpty()) {
                sendMessage(msg)
                editMessage.text.clear()
            }
        }
    }

    private fun connectToServer() {
        scope.launch {
            try {
                // 172.30.1.86 is your Windows PC's IP address on the local network
                Log.d("ChatClient", "Connecting to server...")
                socket = Socket("172.30.1.88", 12345)
                writer = PrintWriter(socket!!.getOutputStream(), true)
                reader = BufferedReader(InputStreamReader(socket!!.getInputStream()))

                Log.d("ChatClient", "Connected!")

                val messageBuffer = StringBuilder()
                // Read Loop
                while (isActive) {
                    val line = reader?.readLine() ?: break
                    Log.d("ChatClient", "Received: $line")

                    if (line.endsWith("<EOM>")) {
                        val realLine = line.replace("<EOM>", "")
                        messageBuffer.append(realLine)
                        val fullMessage = messageBuffer.toString()
                        messageBuffer.clear()

                        withContext(Dispatchers.Main) {
                            // Logic to determine message type
                            val type = if (fullMessage.startsWith("[SYSTEM]")) {
                                ChatMessage.TYPE_SYSTEM
                            } else {
                                ChatMessage.TYPE_OTHER
                            }

                            adapter.addMessage(ChatMessage(fullMessage, type))
                            findViewById<RecyclerView>(R.id.recyclerView).scrollToPosition(messages.size - 0)
                        }
                    } else {
                        messageBuffer.append(line).append("\n")
                    }
                }
            } catch (e: Exception) {
                Log.e("ChatClient", "Connection Error", e)
                withContext(Dispatchers.Main) {
                    adapter.addMessage(ChatMessage("Error: ${e.message}", ChatMessage.TYPE_SYSTEM))
                }
            }
        }
    }

    private fun sendMessage(message: String) {
        scope.launch {
            try {
                writer?.println("$message<EOM>")
                // Add local message as TYPE_ME
                withContext(Dispatchers.Main) {
                    adapter.addMessage(ChatMessage(message, ChatMessage.TYPE_ME))
                    findViewById<RecyclerView>(R.id.recyclerView).scrollToPosition(messages.size - 1)
                }
            } catch (e: Exception) {
                Log.e("ChatClient", "Send Error", e)
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        scope.cancel()
        try {
            socket?.close()
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
}