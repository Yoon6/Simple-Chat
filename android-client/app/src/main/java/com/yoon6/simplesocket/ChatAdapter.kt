package com.yoon6.simplesocket

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView

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
        return messages[position].type.ordinal
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        return when (viewType) {
            MessageType.ME.ordinal -> {
                MeViewHolder(inflater.inflate(R.layout.item_chat_me, parent, false))
            }
            MessageType.SYSTEM.ordinal -> {
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
                holder.textView.text = msg.message
            }
            is OtherViewHolder -> {
                holder.textView.text = msg.message
                holder.nameView.text = "Client ${msg.client}"
            }
            is SystemViewHolder -> {
                holder.textView.text = msg.message
            }
        }
    }

    override fun getItemCount() = messages.size

    fun addMessage(message: ChatMessage) {
        messages.add(message)
        notifyItemInserted(messages.size - 1)
    }
}
