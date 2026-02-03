package com.yoon6.simplesocket

import android.content.Context
import android.graphics.ImageFormat
import android.graphics.Rect
import android.graphics.YuvImage
import android.util.Log
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.ImageProxy
import androidx.camera.core.Preview
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.camera.view.PreviewView
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import java.io.ByteArrayOutputStream
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.nio.ByteBuffer
import java.util.concurrent.Executors

class VideoManager(private val context: Context) {
    private var udpSocket: DatagramSocket? = null
    private val executor = Executors.newSingleThreadExecutor()
    private val scope = CoroutineScope(Dispatchers.IO + Job())
    private var isStreaming = false
    private var targetAddress: InetAddress? = null
    private var targetPort: Int = 12345

    // Sender Stats
    private var sequenceNumber = 0

    // Receiver Stats
    private var lastSequenceNumber = -1
    private var totalPacketsReceived = 0
    private var totalPacketsLost = 0
    private var lastStatTime = 0L
    private var frameCountForFps = 0
    private var totalLatency = 0L

    fun startCamera(lifecycleOwner: LifecycleOwner, previewView: PreviewView) {
        val cameraProviderFuture = ProcessCameraProvider.getInstance(context)

        cameraProviderFuture.addListener({
            val cameraProvider = cameraProviderFuture.get()

            // Preview
            val preview = Preview.Builder()
                .build()
                .also {
                    it.setSurfaceProvider(previewView.surfaceProvider)
                }

            // Image Analysis (Video Stream)
            val imageAnalyzer = ImageAnalysis.Builder()
                .setTargetResolution(android.util.Size(320, 240)) // Lower resolution for UDP
                .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                .build()
                .also {
                    it.setAnalyzer(executor) { image ->
                        processImage(image)
                    }
                }

            // Select Front Camera
            val cameraSelector = CameraSelector.DEFAULT_FRONT_CAMERA

            try {
                cameraProvider.unbindAll()
                cameraProvider.bindToLifecycle(
                    lifecycleOwner, cameraSelector, preview, imageAnalyzer
                )
            } catch (exc: Exception) {
                Log.e("VideoManager", "Use case binding failed", exc)
            }

        }, ContextCompat.getMainExecutor(context))
    }

    fun startStreaming(ip: String, port: Int, remoteView: android.widget.ImageView) {
        scope.launch {
            try {
                // Bind to a specific port is better, but randomized is fine for NAT punch-through if server replies to src
                if (udpSocket == null) {
                    udpSocket = DatagramSocket()
                }
                targetAddress = InetAddress.getByName(ip)
                targetPort = port
                isStreaming = true
                sequenceNumber = 0 // Reset sequence number
                Log.d("VideoManager", "Streaming started to $ip:$port")
                
                // Start Receiving Loop
                startReceiving(remoteView)

            } catch (e: Exception) {
                Log.e("VideoManager", "Error starting stream", e)
            }
        }
    }

    private fun startReceiving(remoteView: android.widget.ImageView) {
        scope.launch(Dispatchers.IO) {
            val buffer = ByteArray(65507) // Max UDP Packet Size
            val packet = DatagramPacket(buffer, buffer.size)
            
            // Stats Init
            lastSequenceNumber = -1
            totalPacketsReceived = 0
            totalPacketsLost = 0
            frameCountForFps = 0
            totalLatency = 0
            lastStatTime = System.currentTimeMillis()

            while (isStreaming && udpSocket != null) {
                try {
                    udpSocket?.receive(packet)
                    
                    val length = packet.length
                    if (length > 12) { // Header is 12 bytes
                        val data = packet.data
                        val byteBuffer = ByteBuffer.wrap(data, 0, 12)
                        val timestamp = byteBuffer.long
                        val seqNum = byteBuffer.int

                        // --- Metrics Calculation ---
                        val latency = System.currentTimeMillis() - timestamp
                        totalLatency += latency
                        
                        if (lastSequenceNumber != -1) {
                            val diff = seqNum - lastSequenceNumber
                            if (diff > 1) {
                                totalPacketsLost += (diff - 1)
                            }
                        }
                        lastSequenceNumber = seqNum
                        totalPacketsReceived++
                        frameCountForFps++

                        // Periodically Log Stats (every 1 second)
                        val now = System.currentTimeMillis()
                        if (now - lastStatTime >= 1000) {
                            val fps = frameCountForFps
                            val avgLatency = if (frameCountForFps > 0) totalLatency / frameCountForFps else 0
                            val lossRate = if (totalPacketsReceived + totalPacketsLost > 0) {
                                (totalPacketsLost.toFloat() / (totalPacketsReceived + totalPacketsLost)) * 100
                            } else 0f

                            Log.i("VideoStats", "FPS: $fps, Latency: ${avgLatency}ms, Loss: ${String.format("%.2f", lossRate)}%")

                            // Reset counters
                            frameCountForFps = 0
                            totalLatency = 0
                            lastStatTime = now
                            // Note: We don't reset totalPacketsLost/Received if we want cumulative loss, 
                            // but for "Current Loss Rate", we should probably reset them or count relative.
                            // Let's reset for interval-based stats.
                            totalPacketsReceived = 0
                            totalPacketsLost = 0
                        }
                        // ---------------------------
                        
                        // Decode JPEG (Payload starts at offset 12)
                        val bitmap = android.graphics.BitmapFactory.decodeByteArray(data, 12, length - 12)
                        
                        // Update UI
                        if (bitmap != null) {
                            // Rotate 90 degrees
                            val matrix = android.graphics.Matrix()
                            matrix.postRotate(-90f)
                            val rotatedBitmap = android.graphics.Bitmap.createBitmap(
                                bitmap, 0, 0, bitmap.width, bitmap.height, matrix, true
                            )
                            
                            scope.launch(Dispatchers.Main) {
                                remoteView.setImageBitmap(rotatedBitmap)
                            }
                        }
                    }
                } catch (e: Exception) {
                    if (isStreaming) {
                        Log.e("VideoManager", "Error receiving packet", e)
                    }
                }
            }
        }
    }

    fun stopStreaming() {
        isStreaming = false
        udpSocket?.close()
        udpSocket = null
    }

    private fun processImage(image: ImageProxy) {
        if (!isStreaming || udpSocket == null || targetAddress == null) {
            image.close()
            return
        }

        try {
            // YUV_420_888 -> NV21 -> JPEG
            val yBuffer = image.planes[0].buffer
            val uBuffer = image.planes[1].buffer
            val vBuffer = image.planes[2].buffer

            val ySize = yBuffer.remaining()
            val uSize = uBuffer.remaining()
            val vSize = vBuffer.remaining()

            // NV21 Byte array size
            val nv21 = ByteArray(ySize + uSize + vSize)

            // U and V are swapped
            yBuffer.get(nv21, 0, ySize)
            vBuffer.get(nv21, ySize, vSize)
            uBuffer.get(nv21, ySize + vSize, uSize)

            val yuvImage = YuvImage(nv21, ImageFormat.NV21, image.width, image.height, null)
            val out = ByteArrayOutputStream()
            
            // Compress to JPEG (Quality 30 to save bandwidth)
            yuvImage.compressToJpeg(Rect(0, 0, image.width, image.height), 30, out)
            val jpegBytes = out.toByteArray()

            // Construct Packet with Header
            // Header: Timestamp (8 bytes) + Sequence Number (4 bytes) = 12 bytes
            val totalSize = 12 + jpegBytes.size
            
            if (totalSize < 60000) {
                 val buffer = ByteBuffer.allocate(totalSize)
                 buffer.putLong(System.currentTimeMillis())
                 buffer.putInt(sequenceNumber++)
                 buffer.put(jpegBytes)
                 
                 val packetData = buffer.array()
                 val packet = DatagramPacket(packetData, packetData.size, targetAddress, targetPort)
                 udpSocket?.send(packet)
            } else {
                Log.w("VideoManager", "Frame too large: $totalSize")
            }

        } catch (e: Exception) {
            Log.e("VideoManager", "Error sending frame", e)
        } finally {
            image.close()
        }
    }
}
