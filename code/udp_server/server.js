const dgram = require('dgram');
const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const server = dgram.createSocket('udp4');

// Set up Express and Socket.io
const app = express();
const httpServer = http.createServer(app);
const io = new Server(httpServer);

const PORT = 8080;
let deviceData = {}; // Store latest active times per device name
let currentLeader = null; // Track the current leader's name
const TEN_MINUTES = 10 * 60 * 1000; // 10 minutes in milliseconds

// Serve static files (like HTML/JS for the frontend)
app.use(express.static('public'));

// Helper function to determine the current leader based on total active time
function getCurrentLeader() {
    let newLeader = null;
    let maxTime = -1;

    for (const name in deviceData) {
        const totalTime = deviceData[name].totalTime; // Use latest reported value

        if (totalTime > maxTime) {
            maxTime = totalTime;
            newLeader = name;
        }
    }
    return newLeader;
}

// Helper function to filter timestamps within the last 10 minutes
function filterRecentTimestamps(timestamps) {
    const cutoff = Date.now() - TEN_MINUTES;
    return timestamps.filter(ts => ts.time >= cutoff);
}

// Event listener for receiving UDP messages
server.on('message', (msg, rinfo) => {
    const messageParts = msg.toString().split(' '); // Split the message
    const time = parseInt(messageParts[0], 10); // Extract numeric value
    const name = messageParts[1]; // Extract device name

    if (isNaN(time) || !name) {
        console.error('Invalid message format:', msg.toString());
        return;
    }

    // Initialize device data if not already present
    if (!deviceData[name]) {
        deviceData[name] = { totalTime: 0, timestamps: [] };
    }

    // Overwrite the latest totalTime with the new value
    deviceData[name].totalTime = time;

    // Store the new timestamp and filter old ones
    deviceData[name].timestamps.push({ time: Date.now(), value: time });
    deviceData[name].timestamps = filterRecentTimestamps(deviceData[name].timestamps);

    console.log(`Received ${time} from ${name} (${rinfo.address}:${rinfo.port})`);

    // Determine the current leader
    const newLeader = getCurrentLeader();
    const leaderChanged = newLeader !== currentLeader ? 1 : 0;
    currentLeader = newLeader;

    console.log(`Leader: ${currentLeader}, Changed: ${leaderChanged}`);

    // Notify the frontend via WebSocket with the latest data
    io.emit('update', deviceData);

    // Send the leader's name to the ESP32
    const response = Buffer.from(JSON.stringify({ leader: currentLeader }));
    server.send(response, rinfo.port, rinfo.address, (error) => {
        if (error) {
            console.error('Error sending response:', error);
        } else {
            console.log(`Sent leader status to ${name}`);
        }
    });
});

// Start the UDP and HTTP servers
server.bind(PORT, () => {
    console.log(`UDP server listening on port ${PORT}`);
});
httpServer.listen(3000, () => {
    console.log('HTTP server listening on port 3000');
});
