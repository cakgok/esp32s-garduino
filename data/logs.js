function fetchLogs() {
    fetch('/api/logs')
        .then(response => response.text())
        .then(html => {
            document.getElementById('logs').innerHTML = html;
        });
}

// Fetch logs immediately and then every 5 seconds
fetchLogs();
setInterval(fetchLogs, 5000);
