package com.yoon6.simplesocket

import android.os.Bundle
import android.widget.Button
import android.widget.ImageView
import androidx.appcompat.app.AppCompatActivity
import androidx.camera.view.PreviewView

class VideoCallActivity : AppCompatActivity() {

    private lateinit var videoManager: VideoManager

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_video_call)

        val remoteView = findViewById<ImageView>(R.id.remoteVideoView)
        val previewView = findViewById<PreviewView>(R.id.viewFinder)
        val btnClose = findViewById<Button>(R.id.btnClose)

        videoManager = VideoManager(this)
        
        // Start Camera & Stream
        previewView.post {
            videoManager.startCamera(this, previewView)
            videoManager.startStreaming(resources.getString(R.string.server_ip), 12345, remoteView)
        }

        btnClose.setOnClickListener {
            finish()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        videoManager.stopStreaming()
    }
}
