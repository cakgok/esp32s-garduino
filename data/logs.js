let socket;
let reconnectInterval = 5000; // 5 seconds
let pingInterval;
let pongTimeout;

function connectWebSocket() {
    socket = new WebSocket('ws://' + location.hostname + ':80/ws');

    socket.onopen = function(e) {
        console.log('Connected to WebSocket');
        startPingInterval();
    };

    socket.onmessage = function(event) {
        const data = JSON.parse(event.data);
        
        if (data.type === 'ping') {
            sendPong();
        } else if (data.type === 'pong') {
            clearTimeout(pongTimeout);
        } else if (data.type === 'log') {
            addLogEntry(data);
        }
    };

    socket.onclose = function(event) {
        console.log('WebSocket connection closed: ', event);
        clearInterval(pingInterval);
        setTimeout(connectWebSocket, reconnectInterval);
    };

    socket.onerror = function(error) {
        console.error('WebSocket error: ', error);
    };
}

function startPingInterval() {
    pingInterval = setInterval(() => {
        if (socket.readyState === WebSocket.OPEN) {
            sendPing();
            setPongTimeout();
        }
    }, 30000); // Send ping every 30 seconds
}

function sendPing() {
    socket.send(JSON.stringify({ type: 'ping' }));
}

function sendPong() {
    socket.send(JSON.stringify({ type: 'pong' }));
}

function setPongTimeout() {
    pongTimeout = setTimeout(() => {
        console.log('Pong not received, closing connection');
        socket.close();
    }, 5000); // Wait 5 seconds for pong before closing
}

// Start the WebSocket connection
connectWebSocket();

// Handle page unload
window.addEventListener('beforeunload', () => {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.close();
    }
});

async function fetchLogs() {
    try {
        const response = await fetch('/api/logs');
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        const data = await response.json();
        
        // Assuming you have a container element with id 'logContainer'
        const logContainer = document.getElementById('log-container');
        logContainer.innerHTML = ''; // Clear existing logs
        
        data.logs.forEach(log => {
            const logEntry = document.createElement('div');
            logEntry.className = `log-entry log-level-${log.level}`;
            logEntry.textContent = `[${log.tag}] ${log.message}`;
            logContainer.appendChild(logEntry);
        });
    } catch (error) {
        console.error('Error fetching logs:', error);
    }
}
document.addEventListener('DOMContentLoaded', () => {
    fetchLogs(); // Initial fetch
    //setInterval(fetchLogs, 10000); // Fetch logs every 10 seconds -- nope, websocket handles it
});

