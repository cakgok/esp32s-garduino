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

eventSource.addEventListener('open', function(event) {
    console.log('SSE connection opened');
});

eventSource.addEventListener('update', function(event) {
    console.log('Received SSE update:', event.data);
    try {
        const data = JSON.parse(event.data);
        updateDashboard(data);
    } catch (error) {
        console.error('Error parsing SSE data:', error);
    }
});

eventSource.addEventListener('error', function(event) {
    if (event.target.readyState === EventSource.CLOSED) {
        console.log('SSE connection closed');
    } else {
        console.error('SSE connection error:', event);
    }
});

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
                // Clear countdown if relay is not active
                clearCountdown(countdownElement);
            }
        }
    }
}

function startCountdown(element, duration) {
    clearCountdown(element); // Clear any existing countdown

    let timeLeft = duration;
    element.textContent = formatTime(timeLeft);
    element.style.display = 'block';

    const countdownInterval = setInterval(() => {
        timeLeft--;
        if (timeLeft < 0) {
            clearInterval(countdownInterval);
            element.style.display = 'none';
            element.dataset.countdownIntervalId = '';
        } else {
            element.textContent = formatTime(timeLeft);
        }
    }, 1000);

    // Store the interval ID on the element
    element.dataset.countdownIntervalId = countdownInterval;
}

function clearCountdown(element) {
    if (element.dataset.countdownIntervalId) {
        clearInterval(parseInt(element.dataset.countdownIntervalId));
        element.dataset.countdownIntervalId = '';
        element.style.display = 'none';
    }
}

document.querySelectorAll('.toggle-switch input[type="checkbox"]').forEach((checkbox, index) => {
    checkbox.addEventListener('change', function() {
        const relayIndex = index;
        const active = this.checked;
        
        const payload = JSON.stringify({ relay: relayIndex, active: active });
        console.log('Sending payload:', payload);

        fetch('/api/relay', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: payload,
        })
        .then(response => response.json())
        .then(data => {
            console.log('Parsed server response:', data);
            if (data.success) {
                console.log(`Relay ${data.relayIndex} ${data.message}`);
                const countdownElement = document.getElementById(`countdown${relayIndex + 1}`);
                
                if (active && data.activationPeriod) {
                    const activationPeriodInSeconds = Math.floor(data.activationPeriod / 1000);
                    startCountdown(countdownElement, activationPeriodInSeconds);
                } else {
                    clearCountdown(countdownElement);
                }
            } else {
                console.error('Relay toggle failed:', data.message);
                this.checked = !active; // Revert the slider
            }
        })
        .catch(error => {
            console.error('Fetch error:', error);
            this.checked = !active; // Revert the slider
            alert('Failed to toggle relay. Please try again.');
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