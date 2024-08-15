let currentConfig = {};
let hasChanges = false;

document.addEventListener('DOMContentLoaded', fetchConfig);

async function fetchConfig() {
    try {
        const response = await fetch('/api/config');
        if (!response.ok) {
            throw new Error('Failed to fetch configuration');
        }
        currentConfig = await response.json();
        populateForm(currentConfig);
    } catch (error) {
        console.error('Error:', error);
        alert('Failed to load configuration. Please try again.');
    }
}

function populateGlobalSettings(config) {
    const elements = [
        { id: 'currentTempOffset', value: config.temperatureOffset.toFixed(1) },
        { id: 'currentTelemetryInterval', value: formatDuration(config.telemetryInterval) },
        { id: 'currentSensorUpdateInterval', value: formatDuration(config.sensorUpdateInterval) },
        { id: 'currentLcdUpdateInterval', value: formatDuration(config.lcdUpdateInterval) },
        { id: 'currentSensorPublishInterval', value: formatDuration(config.sensorPublishInterval) },
        { id: 'tempOffset', value: config.temperatureOffset },
        { id: 'telemetryInterval', value: config.telemetryInterval / 1000 },
        { id: 'sensorUpdateInterval', value: config.sensorUpdateInterval / 1000 },
        { id: 'lcdUpdateInterval', value: config.lcdUpdateInterval / 1000 },
        { id: 'sensorPublishInterval', value: config.sensorPublishInterval / 1000 }
    ];

    elements.forEach(({ id, value }) => {
        const element = document.getElementById(id);
        if (element) {
            if (element.tagName === 'INPUT') {
                element.value = value;
            } else {
                element.textContent = value;
            }
        }
    });
}

// Function to create HTML for a single sensor configuration
function createSensorConfigHTML(sensorConfig, index) {
    const sensorDiv = document.createElement('div');
    sensorDiv.className = 'sensor-config';
    sensorDiv.innerHTML = `
        <h3>Sensor ${index + 1}</h3>
        <table>
            <tr>
                <th>Setting</th>
                <th>Current Value</th>
                <th>New Value</th>
                <th>Default Value</th>
            </tr>
            <tr>
                <td>
                    <label for="threshold_${index}">Threshold</label>
                    <span class="tooltip">Range: 5.0 to 50.0</span>
                </td>
                <td id="currentThreshold_${index}">${sensorConfig.threshold.toFixed(1)}</td>
                <td class="input-cell"><div class="input-wrapper"><input type="number" id="threshold_${index}" step="0.1" min="5" max="50" value="${sensorConfig.threshold}"></div></td>
                <td>25.0</td>
            </tr>
            <tr>
                <td>
                    <label for="activationPeriod_${index}">Activation Period</label>
                    <span class="tooltip">Range: 1 to 60 seconds</span>
                </td>
                <td id="currentActivationPeriod_${index}">${formatDuration(sensorConfig.activationPeriod)}</td>
                <td class="input-cell"><div class="input-wrapper"><input type="number" id="activationPeriod_${index}" step="1" min="1" max="60" value="${sensorConfig.activationPeriod / 1000}"></div></td>
                <td>30 seconds</td>
            </tr>
            <tr>
                <td>
                    <label for="wateringInterval_${index}">Watering Interval</label>
                    <span class="tooltip">Range: 1 to 120 hours</span>
                </td>
                <td id="currentWateringInterval_${index}">${formatDuration(sensorConfig.wateringInterval)}</td>
                <td class="input-cell"><div class="input-wrapper"><input type="number" id="wateringInterval_${index}" step="1" min="1" max="120" value="${sensorConfig.wateringInterval / 3600000}"></div></td>
                <td>24 hours</td>
            </tr>
        </table>
    `;
    return sensorDiv;
}

// Function to create and populate sensor configurations
function createAndPopulateSensorConfigs(config) {
    const sensorConfigsContainer = document.getElementById('sensorConfigs');
    sensorConfigsContainer.innerHTML = ''; // Clear existing content

    config.sensorConfigs.forEach((sensorConfig, index) => {
        const sensorDiv = createSensorConfigHTML(sensorConfig, index);
        sensorConfigsContainer.appendChild(sensorDiv);
    });
}

// Function to add event listeners for change detection
function addChangeListeners() {
    document.querySelectorAll('input').forEach(input => {
        input.addEventListener('input', () => {
            hasChanges = true;
            updateSaveButton();
        });
    });
}

// Main function to populate the form
function populateForm(config) {
    populateGlobalSettings(config);
    createAndPopulateSensorConfigs(config);
    addChangeListeners();

    hasChanges = false;
    updateSaveButton();
}

function formatDuration(ms) {
    const seconds = ms / 1000;
    if (seconds < 60) {
        return `${seconds.toFixed(1)} seconds`;
    } else if (seconds < 3600) {
        return `${(seconds / 60).toFixed(1)} minutes`;
    } else {
        return `${(seconds / 3600).toFixed(1)} hours`;
    }
}

function updateSaveButton() {
    const saveButton = document.getElementById('saveButton');
    saveButton.classList.toggle('active', hasChanges);
}

async function saveConfig() {
    const newConfig = {
        temperatureOffset: parseFloat(document.getElementById('tempOffset').value),
        telemetryInterval: parseInt(document.getElementById('telemetryInterval').value) * 1000,
        sensorUpdateInterval: parseInt(document.getElementById('sensorUpdateInterval').value) * 1000,
        lcdUpdateInterval: parseInt(document.getElementById('lcdUpdateInterval').value) * 1000,
        sensorPublishInterval: parseInt(document.getElementById('sensorPublishInterval').value) * 1000,
        sensorConfigs: currentConfig.sensorConfigs.map((_, index) => ({
            threshold: parseFloat(document.getElementById(`threshold_${index}`).value),
            activationPeriod: parseInt(document.getElementById(`activationPeriod_${index}`).value) * 1000,
            wateringInterval: parseInt(document.getElementById(`wateringInterval_${index}`).value) * 3600000 // Convert from hours to milliseconds
        }))
    };

    try {
        const response = await fetch('/api/config', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ config: newConfig }),
        });

        if (!response.ok) {
            const errorText = await response.text();
            throw new Error(`Failed to save configuration: ${errorText}`);
        }

        alert('Configuration saved successfully');
        fetchConfig(); // Refresh the displayed configuration
    } catch (error) {
        console.error('Error:', error);
        alert(`Failed to save configuration: ${error.message}`);
    }
}

async function resetToDefault() {
    if (confirm('Are you sure you want to reset to default settings? This action cannot be undone.')) {
        try {
            const response = await fetch('/api/defaultConfig');
            if (!response.ok) {
                throw new Error('Failed to fetch default configuration');
            }
            const defaultConfig = await response.json();
            populateForm(defaultConfig);
        } catch (error) {
            console.error('Error:', error);
            alert('Failed to load default configuration. Please try again.');
        }
    }
}