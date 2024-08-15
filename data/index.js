// Utility function to format time
function formatTime(seconds) {
    const minutes = Math.floor(seconds / 60);
    const remainingSeconds = seconds % 60;
    return `${minutes}:${remainingSeconds.toString().padStart(2, '0')}`;
}

// Initialize EventSource for server-sent events
const eventSource = new EventSource('/api/events');

// Handle incoming server events
eventSource.onmessage = function(event) {
    const data = JSON.parse(event.data);
    updateDashboard(data);
};

// Update dashboard with new data
function updateDashboard(data) {
    // Update plant moisture levels
    for (let i = 0; i < 4; i++) {
        const moistureElement = document.getElementById(`plant${i+1}-humidity`);
        const moistureTextElement = document.getElementById(`plant${i+1}-humidity-text`);
        
        if (data.plants[i]) {
            const moisture = data.plants[i].moisture;
            moistureElement.style.width = `${moisture}%`;
            moistureTextElement.textContent = `Humidity: ${moisture.toFixed(1)}%`;
        }
    }

    // Update temperature and pressure
    document.getElementById('temperature').textContent = `${data.temperature.toFixed(1)}°C`;
    document.getElementById('pressure').textContent = `${data.pressure.toFixed(1)} hPa`;

    // Update water level indicator
    const waterLevelElement = document.getElementById('water-level-indicator');
    waterLevelElement.textContent = data.waterLevel ? "Water level OK" : "Low water level";
    waterLevelElement.style.color = data.waterLevel ? "green" : "red";

    // Update relay states
    for (let i = 0; i < 4; i++) {
        const sliderElement = document.getElementById(`relay${i+1}`);
        const countdownElement = document.getElementById(`countdown${i+1}`);
        
        if (data.relays[i]) {
            const relay = data.relays[i];
            sliderElement.checked = relay.active;
            
            if (relay.active && relay.activationTime) {
                countdownElement.textContent = formatTime(relay.activationTime);
                countdownElement.style.display = 'block';

                // Start countdown
                startCountdown(countdownElement, relay.activationTime);
            } else {
                countdownElement.style.display = 'none';
            }
        }
    }
}

function startCountdown(element, duration) {
    let timeLeft = duration;
    const countdownInterval = setInterval(() => {
        timeLeft--;
        if (timeLeft <= 0) {
            clearInterval(countdownInterval);
            element.style.display = 'none';
        } else {
            element.textContent = formatTime(timeLeft);
        }
    }, 1000);
}

document.querySelectorAll('.slider').forEach((slider, index) => {
    slider.addEventListener('change', function() {
        const relayIndex = index + 1;
        const active = this.checked;
        
        fetch('/api/relay', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ relay: relayIndex, active: active }),
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                if (data.active && data.activationTime) {
                    const countdownElement = document.getElementById(`countdown${relayIndex}`);
                    startCountdown(countdownElement, data.activationTime);
                }
            } else {
                // Revert the slider if the action was unsuccessful
                this.checked = !active;
            }
        })
        .catch(error => {
            console.error('Error:', error);
            // Revert the slider on error
            this.checked = !active;
        });
    });
});


// Initial dashboard update
fetch('/api/sensorData')
    .then(response => response.json())
    .then(data => {
        console.log('Received sensor data:', data);
        updateDashboard(data);
    })