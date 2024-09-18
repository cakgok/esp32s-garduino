// WebSocket connection
let socket;
let reconnectAttempts = 0;
let maxReconnectAttempts = 5;
let reconnectInterval = 5000; // 5 seconds
let pingInterval;
let pongTimeout;
let connectionId = generateUniqueId(); // Implement this function to generate a unique ID

function generateUniqueId() {
    return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
        var r = Math.random() * 16 | 0, v = c == 'x' ? r : (r & 0x3 | 0x8);
        return v.toString(16);
    });
}

function connectWebSocket() {
    if (socket && (socket.readyState === WebSocket.CONNECTING || socket.readyState === WebSocket.OPEN)) {
        console.log('WebSocket is already connecting or open');
        return;
    }

    socket = new WebSocket(`ws://${location.hostname}:80/ws?id=${connectionId}`);

    socket.onopen = function(e) {
        console.log('Connected to WebSocket');
        reconnectAttempts = 0;
        startPingInterval();
    };

    socket.onmessage = function(event) {
        const data = JSON.parse(event.data);
        
        if (data.type === 'log') {
            addLogEntry(data);
        } else if (data.type === 'ping') {
            sendPong();
        } else if (data.type === 'pong') {
            clearTimeout(pongTimeout);
        }
    };

    socket.onclose = function(event) {
        console.log('WebSocket connection closed: ', event);
        clearInterval(pingInterval);
        if (reconnectAttempts < maxReconnectAttempts) {
            let delay = Math.min(30000, (reconnectAttempts + 1) * reconnectInterval);
            setTimeout(connectWebSocket, delay);
            reconnectAttempts++;
        } else {
            console.log('Max reconnection attempts reached');
        }
    };

    socket.onerror = function(error) {
        console.error('WebSocket error: ', error);
    };
}

function startPingInterval() {
    clearInterval(pingInterval); // Clear any existing interval
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
    clearTimeout(pongTimeout); // Clear any existing timeout
    pongTimeout = setTimeout(() => {
        console.log('Pong not received, closing connection');
        socket.close();
    }, 5000); // Wait 5 seconds for pong before closing
}

function handleVisibilityChange() {
    if (document.hidden) {
        if (socket && socket.readyState === WebSocket.OPEN) {
            socket.send(JSON.stringify({ type: 'pause' }));
            clearInterval(pingInterval);
        }
    } else {
        if (socket && socket.readyState === WebSocket.OPEN) {
            socket.send(JSON.stringify({ type: 'resume' }));
            startPingInterval();
        } else {
            connectWebSocket();
        }
    }
}

// Function to add log entries (unchanged from your original code)
function addLogEntry(log) {
    const logContainer = document.getElementById('log-container');
    const logEntry = document.createElement('div');
    logEntry.className = `log-entry log-level-${log.level}`;
    logEntry.textContent = `[${log.tag}] ${getLevelString(log.level)}: ${log.message}`;
    logContainer.appendChild(logEntry);
    
    // Scroll to the bottom of the container
    logContainer.scrollTop = logContainer.scrollHeight;
}

// Function to get level string (unchanged from your original code)
function getLevelString(level) {
    switch(level) {
        case 0: return "DEBUG";
        case 1: return "INFO";
        case 2: return "WARNING";
        case 3: return "ERROR";
        default: return "UNKNOWN";
    }
}

// Function to fetch logs (unchanged from your original code)
async function fetchLogs() {
    const logContainer = document.getElementById('log-container');
    logContainer.innerHTML = ''; // Clear existing logs

    async function fetchSingleLog() {
        try {
            const response = await fetch('/api/logs');
            if (response.status === 204) {
                // No more logs
                return null;
            }
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return await response.json();
        } catch (error) {
            console.error('Error fetching log:', error);
            return null;
        }
    }

    async function fetchAndDisplayLogs() {
        let log;
        do {
            log = await fetchSingleLog();
            if (log) {
                addLogEntry(log);
            }
        } while (log);
    }

    await fetchAndDisplayLogs();
}

// Initialize WebSocket connection and set up event listeners
document.addEventListener('DOMContentLoaded', () => {
    fetchLogs(); // Initial fetch
    connectWebSocket(); // Initial WebSocket connection
});

function pauseWebSocket() {
    if (socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ type: 'pause' }));
    }
}

function resumeWebSocket() {
    if (socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ type: 'resume' }));
    }
}

document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
        pauseWebSocket();
    } else {
        resumeWebSocket();
    }
});