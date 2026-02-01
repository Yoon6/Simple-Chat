#pragma once
#include <string>

std::string escapeString(const std::string& input) {
    std::string output;
    // 최적화: 원본 길이보다 약간 더 넉넉하게 잡아 재할당(Reallocation) 방지
    output.reserve(input.length() * 1.1); 

    for (char c : input) {
        switch (c) {
            case '"':
                output += "\\\""; // " 를 \" 로 변환
                break;
            case '\n':
                output += "\\n";  // 줄바꿈을 문자열 "\n"으로 변환
                break;
            case '\\': 
                // [중요] 백슬래시(\) 자체도 \\로 바꿔줘야 JSON이 깨지지 않습니다.
                // 예: "C:\User" -> "C:\\User"
                output += "\\\\"; 
                break;
            case '\r':
                // 윈도우(\r\n)의 경우 \r은 보통 무시하거나 \\r로 바꿉니다.
                break;
            case '\t':
                output += "\\t";  // 탭 문자 (필요시 추가)
                break;
            default:
                output += c;
                break;
        }
    }
    return output;
}

enum class ChatType
{
    SYSTEM,
    CLIENT
};

typedef struct ChatPacket {
    int client;
    ChatType type;
    std::string message;

    ChatPacket(int client, ChatType type, std::string message)
        : client(client), type(type), message(message) {}

    std::string toJsonString()
    {
        message = escapeString(message);
        return "{\"client\": " + std::to_string(client) + ", \"type\": " + (type == ChatType::SYSTEM ? "\"system\"" : "\"client\"") + ", \"message\": " + "\"" + message + "\"" + "}\n";
    }
} ChatPacket;