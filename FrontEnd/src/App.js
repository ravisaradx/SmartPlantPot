import React, { useState, useEffect, useRef } from "react";
import * as mobilenet from '@tensorflow-models/mobilenet';
import '@tensorflow/tfjs'; // Import TensorFlow.js

const App = () => {
  // State for sensor data received via WebSocket
  const [data, setData] = useState({
    soil_moisture: null,
    water_level: null,
    light: null,
    humidity: null,
  });

  // State for WebSocket connection status
  const [enemies, setEnemies] = useState(false);
  const [connected, setConnected] = useState(false);

  // State for image classification predictions
  const [predictions, setPredictions] = useState([]);
  
  // Refs for image and canvas elements
  const imgRef = useRef(null); // Reference to the img DOM element
  const canvasRef = useRef(null); // Reference to a canvas to grab image frames
  const modelRef = useRef(null); // To store the loaded MobileNet model
  const lastImageTimeRef = useRef(Date.now()); // To track the time of the last image classification

  // WebSocket connection setup
  useEffect(() => {
    console.log("Connecting to WebSocket...");
    const socket = new WebSocket("ws://172.20.10.5:81");

    socket.onopen = () => {
      console.log("Connected to WebSocket server");
      setConnected(true);
    };

    socket.onmessage = (event) => {
      const receivedData = JSON.parse(event.data);
      console.log("Received:", receivedData);
      setData((prevData) => ({ ...prevData, ...receivedData }));
    };

    socket.onerror = (error) => {
      console.error("WebSocket error:", error);
    };

    socket.onclose = () => {
      console.log("Disconnected from WebSocket server");
      setConnected(false);
    };

    for (const e in predictions) {
     if (e.className.contains('caterpillar') && Math.round(e.probability * 100) > 50) {
      setEnemies(true)
      alert('ENIMIEEE')
     } else {
      setEnemies(false)
     }
    }

    return () => {
      socket.close();
    };
  }, []);

  // Control motor function via WebSocket
  const controlMotor = (command) => {
    const socket = new WebSocket("ws://172.20.10.5:81");
    socket.onopen = () => {
      socket.send(command);
      socket.close();
    };
  };

  // Load MobileNet model and start image classification
  useEffect(() => {
    if (imgRef.current && canvasRef.current) {
      mobilenet.load().then((model) => {
        modelRef.current = model;
        console.log("MobileNet model loaded");

        const analyzeFrame = async () => {
          if (imgRef.current.complete) {
            const ctx = canvasRef.current.getContext('2d');
            ctx.drawImage(imgRef.current, 0, 0, canvasRef.current.width, canvasRef.current.height);

            // Only classify every 1 second
            const currentTime = Date.now();
            if (currentTime - lastImageTimeRef.current > 1000) {
              lastImageTimeRef.current = currentTime; // Update the last classification time
              const imageData = ctx.getImageData(0, 0, canvasRef.current.width, canvasRef.current.height);
              const newPredictions = await modelRef.current.classify(imageData);
              setPredictions(newPredictions);
            }
          }
          requestAnimationFrame(analyzeFrame); // Continue analyzing frames
        };

        analyzeFrame(); // Start analyzing frames
      });
    }
  }, []);

  return (
    <div className="App">
      {
        enemies ?
        <h1>WE FOUND ENIMIE!!!!</h1>
         : null
      }


      <h1>ESP32 Real-Time Monitoring and MobileNet Classification</h1>

      {/* WebSocket Connection Status */}
      {connected ? (
        <div>
          <h2>Connected to WebSocket</h2>
          <p>Soil Moisture: {data.soil_moisture ?? "Loading..."}</p>
          <p>Water Level: {data.water_level ?? "Loading..."}%</p>
          <p>Light: {data.light === 1 ? "Bright" : "Dark"}</p>
          <p>Humidity: {data.humidity ?? "Loading..."}%</p>
          <button onClick={() => controlMotor("MOTOR_ON")}>Turn Motor ON</button>
        </div>
      ) : (
        <p>Connecting to WebSocket...</p>
      )}

      {/* MJPEG Stream from ESP32 Camera */}
      <img
        ref={imgRef}
        src="http://172.20.10.8:81/stream" // MJPEG stream URL
        alt="Live stream"
        width="640"
        height="480"
        style={{
          border: '2px solid #ccc',
          borderRadius: '8px',
        }}
        crossOrigin="anonymous" // Enable CORS
        onLoad={() => console.log('Image loaded successfully')}
        onError={() => console.error('Failed to load image')}
      />

      {/* Canvas to grab image data from MJPEG stream for classification */}
      <canvas
        ref={canvasRef} 
        width="640"
        height="480"
        style={{ display: 'none' }} // Hide the canvas element
      ></canvas>

      {/* Display Image Classification Results */}
      <div>
        <h2>Predictions:</h2>
        <ul>
          {predictions.map((prediction, idx) => (
            <li key={idx}>
              {prediction.className} - {Math.round(prediction.probability * 100)}%
            </li>
          ))}
        </ul>
      </div>
    </div>
  );
};

export default App;