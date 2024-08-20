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
        { id: 'telemetryInterval', value: config.telemetryInterval },
        { id: 'sensorUpdateInterval', value: config.sensorUpdateInterval },
        { id: 'lcdUpdateInterval', value: config.lcdUpdateInterval },
        { id: 'sensorPublishInterval', value: config.sensorPublishInterval }
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

function createSensorConfigHTML(sensorConfig, index) {
    const sensorDiv = document.createElement('div');
    sensorDiv.className = `sensor-config ${sensorConfig.sensorEnabled ? '' : 'disabled'}`;
    sensorDiv.innerHTML = `
        <h3>Sensor ${index + 1}</h3>
        <div class="checkbox-wrapper">
            <input type="checkbox" id="sensorEnabled_${index}" ${sensorConfig.sensorEnabled ? 'checked' : ''}>
            <label for="sensorEnabled_${index}">Enable Sensor</label>
        </div>
        <div class="checkbox-wrapper">
            <input type="checkbox" id="relayEnabled_${index}" ${sensorConfig.relayEnabled ? 'checked' : ''}>
            <label for="relayEnabled_${index}">Enable Relay Control</label>
        </div>
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
                    <label for="activationPeriod_${index}">Activation Period (ms)</label>
                    <span class="tooltip">Range: 1000 to 60000</span>
                </td>
                <td id="currentActivationPeriod_${index}">${formatDuration(sensorConfig.activationPeriod)}</td>
                <td class="input-cell"><div class="input-wrapper"><input type="number" id="activationPeriod_${index}" step="1000" min="1000" max="60000" value="${sensorConfig.activationPeriod}"></div></td>
                <td>30000</td>
            </tr>
            <tr>
                <td>
                    <label for="wateringInterval_${index}">Watering Interval (ms)</label>
                    <span class="tooltip">Range: 3600000 to 432000000</span>
                </td>
                <td id="currentWateringInterval_${index}">${formatDuration(sensorConfig.wateringInterval)}</td>
                <td class="input-cell"><div class="input-wrapper"><input type="number" id="wateringInterval_${index}" step="3600000" min="3600000" max="432000000" value="${sensorConfig.wateringInterval}"></div></td>
                <td>86400000</td>
            </tr>
            <tr>
                <td>
                    <label for="sensorPin_${index}">Sensor Pin</label>
                </td>
                <td id="currentSensorPin_${index}">${sensorConfig.sensorPin}</td>
                <td class="input-cell"><div class="input-wrapper"><input type="number" id="sensorPin_${index}" min="0" max="40" value="${sensorConfig.sensorPin}"></div></td>
                <td>-</td>
            </tr>
            <tr>
                <td>
                    <label for="relayPin_${index}">Relay Pin</label>
                </td>
                <td id="currentRelayPin_${index}">${sensorConfig.relayPin}</td>
                <td class="input-cell"><div class="input-wrapper"><input type="number" id="relayPin_${index}" min="0" max="40" value="${sensorConfig.relayPin}"></div></td>
                <td>-</td>
            </tr>
        </table>
    `;
    return sensorDiv;
}

function createAndPopulateSensorConfigs(config) {
    const sensorConfigsContainer = document.getElementById('sensorConfigs');
    sensorConfigsContainer.innerHTML = '';

    config.sensorConfigs.forEach((sensorConfig, index) => {
        const sensorDiv = createSensorConfigHTML(sensorConfig, index);
        sensorConfigsContainer.appendChild(sensorDiv);
    });
}

function addChangeListeners() {
    document.querySelectorAll('input').forEach(input => {
        input.addEventListener('input', () => {
            hasChanges = true;
            updateSaveButton();
            if (input.type === 'checkbox' && input.id.startsWith('sensorEnabled_')) {
                const index = input.id.split('_')[1];
                const sensorDiv = document.querySelector(`.sensor-config:nth-child(${parseInt(index) + 1})`);
                sensorDiv.classList.toggle('disabled', !input.checked);
            }
        });
    });
}

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
    const saveButton = document.getElementById('saveConfig');
    saveButton.classList.toggle('active', hasChanges);
}

async function saveConfig() {
    const newConfig = {
        temperatureOffset: parseFloat(document.getElementById('tempOffset').value),
        telemetryInterval: parseInt(document.getElementById('telemetryInterval').value),
        sensorUpdateInterval: parseInt(document.getElementById('sensorUpdateInterval').value),
        lcdUpdateInterval: parseInt(document.getElementById('lcdUpdateInterval').value),
        sensorPublishInterval: parseInt(document.getElementById('sensorPublishInterval').value),
        sensorConfigs: currentConfig.sensorConfigs.map((_, index) => ({
            threshold: parseFloat(document.getElementById(`threshold_${index}`).value),
            activationPeriod: parseInt(document.getElementById(`activationPeriod_${index}`).value),
            wateringInterval: parseInt(document.getElementById(`wateringInterval_${index}`).value),
            sensorEnabled: document.getElementById(`sensorEnabled_${index}`).checked,
            relayEnabled: document.getElementById(`relayEnabled_${index}`).checked,
            sensorPin: parseInt(document.getElementById(`sensorPin_${index}`).value),
            relayPin: parseInt(document.getElementById(`relayPin_${index}`).value)
        }))
    };

    try {
        const response = await fetch('/api/config', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(newConfig),
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
            const response = await fetch('/api/resetToDefault', {
                method: 'POST',
            });
            if (!response.ok) {
                throw new Error('Failed to reset to default configuration');
            }
            alert('Configuration reset to default successfully');
            fetchConfig(); // Refresh the displayed configuration
        } catch (error) {
            console.error('Error:', error);
            alert('Failed to reset to default configuration. Please try again.');
        }
    }
}

document.getElementById('saveConfig').addEventListener('click', saveConfig);
document.getElementById('resetToDefault').addEventListener('click', resetToDefault);