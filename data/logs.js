let socket;
let reconnectInterval = 5000; // 5 seconds
let pingInterval;
let pongTimeout;
let isClosing = false; // Flag to prevent reconnection attempts during intentional closes

function connectWebSocket() {
    if (isClosing) return; // Don't attempt to reconnect if we're intentionally closing

    socket = new WebSocket('ws://' + location.hostname + ':80/ws');

    socket.onopen = function(e) {
        console.log('Connected to WebSocket');
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
        if (!isClosing) {
            setTimeout(connectWebSocket, reconnectInterval);
        }
    };

    socket.onerror = function(error) {
        console.error('WebSocket error: ', error);
    };
}

function closeWebSocket() {
    isClosing = true;
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.close();
    }
    clearInterval(pingInterval);
    clearTimeout(pongTimeout);
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

//stream new logs throught the ws
function addLogEntry(log) {
    const logContainer = document.getElementById('log-container');
    const logEntry = document.createElement('div');
    logEntry.className = `log-entry log-level-${log.level}`;
    logEntry.textContent = `[${log.tag}] ${getLevelString(log.level)}: ${log.message}`;
    logContainer.appendChild(logEntry);
    
    // Scroll to the bottom of the container
    logContainer.scrollTop = logContainer.scrollHeight;
}
// Start the WebSocket connection
connectWebSocket();

// Handle page unload
window.addEventListener('beforeunload', closeWebSocket);
// Handle page visibility change
document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
        closeWebSocket();
    } else {
        isClosing = false;
        connectWebSocket();
    }
});

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
                const logEntry = document.createElement('div');
                logEntry.className = `log-entry log-level-${log.level}`;
                const levelString = getLevelString(log.level);
                logEntry.textContent = `[${log.tag}] ${levelString}: ${log.message}`;
                logContainer.appendChild(logEntry);
                
                // Scroll to the bottom of the container
                logContainer.scrollTop = logContainer.scrollHeight;
            }
        } while (log);
    }

    await fetchAndDisplayLogs();
}

function getLevelString(level) {
    switch(level) {
        case 0: return "DEBUG";
        case 1: return "INFO";
        case 2: return "WARNING";
        case 3: return "ERROR";
        default: return "UNKNOWN";
    }
}

document.addEventListener('DOMContentLoaded', () => {
    fetchLogs(); // Initial fetch
    //setInterval(fetchLogs, 10000); // Fetch logs every 10 seconds -- nope, websocket handles it
});

