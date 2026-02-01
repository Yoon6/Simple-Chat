package com.yoon6.simplesocket
import kotlinx.serialization.Serializable
import kotlinx.serialization.SerialName

@Serializable
enum class MessageType {
    @SerialName("system") SYSTEM,
    @SerialName("client") CLIENT,
    @SerialName("me") ME
}

@Serializable
data class ChatMessage(
    val client: Int,
    val type: MessageType,
    val message: String
)
