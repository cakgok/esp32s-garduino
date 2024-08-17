const socket = new WebSocket('ws://' + location.hostname + ':80/ws');

socket.onopen = function(e) {
    console.log('Connected to WebSocket');
};

socket.onmessage = function(event) {
    const log = JSON.parse(event.data);
    addLogEntry(log);
};

function addLogEntry(log) {
    const logContainer = document.getElementById('log-container');
    const logEntry = document.createElement('div');
    logEntry.className = 'log-entry ' + log.level.toLowerCase();
    logEntry.innerHTML = `<span class="tag">[${log.tag}]</span> <span class="level">${log.level}</span>: ${log.message}`;
    logContainer.appendChild(logEntry);
    logContainer.scrollTop = logContainer.scrollHeight;
}

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

