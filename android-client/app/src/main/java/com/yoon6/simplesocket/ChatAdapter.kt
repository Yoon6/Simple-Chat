package com.yoon6.simplesocket

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView

// Data Model
data class ChatMessage(val content: String, val type: Int) {
    companion object {
        const val TYPE_ME = 0
        const val TYPE_OTHER = 1
        const val TYPE_SYSTEM = 2
    }
}

class ChatAdapter(private val messages: MutableList<ChatMessage>) : RecyclerView.Adapter<RecyclerView.ViewHolder>() {

    class MeViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        val textView: TextView = view.findViewById(R.id.textMessage)
    }

    class OtherViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        val textView: TextView = view.findViewById(R.id.textMessage)
        val nameView: TextView = view.findViewById(R.id.textName)
    }

    class SystemViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        val textView: TextView = view.findViewById(R.id.textSystem)
    }

    override fun getItemViewType(position: Int): Int {
        return messages[position].type
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        return when (viewType) {
            ChatMessage.TYPE_ME -> {
                MeViewHolder(inflater.inflate(R.layout.item_chat_me, parent, false))
            }
            ChatMessage.TYPE_SYSTEM -> {
                SystemViewHolder(inflater.inflate(R.layout.item_chat_system, parent, false))
            }
            else -> {
                OtherViewHolder(inflater.inflate(R.layout.item_chat_other, parent, false))
            }
        }
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        val msg = messages[position]
        when (holder) {
            is MeViewHolder -> {
                holder.textView.text = msg.content
            }
            is OtherViewHolder -> {
                holder.textView.text = msg.content
                // holder.nameView.text = "Friend" // Default from XML is fine
            }
            is SystemViewHolder -> {
                // Strip "[SYSTEM] " if present for cleaner look
                val cleanText = msg.content.replace("[SYSTEM]", "").trim()
                holder.textView.text = cleanText
            }
        }
    }

    override fun getItemCount() = messages.size

    fun addMessage(message: ChatMessage) {
        messages.add(message)
        notifyItemInserted(messages.size - 1)
    }
}
