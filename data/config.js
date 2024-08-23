let currentConfig = {};
let hasChanges = false;
document.addEventListener('DOMContentLoaded', fetchConfig);

async function fetchConfig() {
    try {
        const response = await fetch('/api/config');
        if (!response.ok) {
            throw new Error('Failed to fetch configuration');
        }
        const rawData = await response.text();
        console.log('Raw config data:', rawData);
        
        try {
            currentConfig = JSON.parse(rawData);
            console.log('Parsed config:', JSON.stringify(currentConfig, null, 2));
            
            if (currentConfig.hasOwnProperty('sensorConfigs')) {
                console.log('sensorConfigs property exists');
                console.log('Type of sensorConfigs:', typeof currentConfig.sensorConfigs);
                if (Array.isArray(currentConfig.sensorConfigs)) {
                    console.log('Number of sensor configs:', currentConfig.sensorConfigs.length);
                    currentConfig.sensorConfigs.forEach((config, index) => {
                        console.log(`Sensor config ${index}:`, JSON.stringify(config, null, 2));
                    });
                } else {
                    console.error('sensorConfigs is not an array:', currentConfig.sensorConfigs);
                }
            } else {
                console.error('sensorConfigs property is missing from the config');
            }
            
            populateForm(currentConfig);
        } catch (parseError) {
            console.error('Error parsing JSON:', parseError);
            console.log('Problematic JSON string:', rawData);
        }
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
    console.log(`Creating HTML for sensor config ${index}:`, sensorConfig);
    const sensorDiv = document.createElement('div');
    sensorDiv.className = `sensor-config ${sensorConfig && sensorConfig.sensorEnabled ? '' : 'disabled'}`;
    sensorDiv.innerHTML = `
        <h3>Sensor ${index + 1}</h3>
        <div class="checkbox-wrapper">
            <input type="checkbox" id="sensorEnabled_${index}" ${sensorConfig && sensorConfig.sensorEnabled ? 'checked' : ''}>
            <label for="sensorEnabled_${index}">Enable Sensor</label>
        </div>
        <div class="checkbox-wrapper">
            <input type="checkbox" id="relayEnabled_${index}" ${sensorConfig && sensorConfig.relayEnabled ? 'checked' : ''}>
            <label for="relayEnabled_${index}">Enable Relay Control</label>
        </div>
        <table>
            <tr>
                <th>Setting</th>
                <th>Current Value</th>
                <th>New Value</th>
            </tr>
            <tr>
                <td>Threshold</td>
                <td id="currentThreshold_${index}">${sensorConfig ? sensorConfig.threshold.toFixed(1) : 'N/A'}</td>
                <td class="input-cell"><div class="input-wrapper"><input type="number" id="threshold_${index}" step="0.1" min="5" max="50" value="${sensorConfig ? sensorConfig.threshold : ''}"></div></td>
            </tr>
            <tr>
                <td>Activation Period (ms)</td>
                <td id="currentActivationPeriod_${index}">${sensorConfig ? sensorConfig.activationPeriod : 'N/A'}</td>
                <td class="input-cell"><div class="input-wrapper"><input type="number" id="activationPeriod_${index}" step="1000" min="1000" max="60000" value="${sensorConfig ? sensorConfig.activationPeriod : ''}"></div></td>
            </tr>
            <tr>
                <td>Watering Interval (ms)</td>
                <td id="currentWateringInterval_${index}">${sensorConfig ? sensorConfig.wateringInterval : 'N/A'}</td>
                <td class="input-cell"><div class="input-wrapper"><input type="number" id="wateringInterval_${index}" step="3600000" min="3600000" max="432000000" value="${sensorConfig ? sensorConfig.wateringInterval : ''}"></div></td>
            </tr>
            <tr>
                <td>Sensor Pin</td>
                <td id="currentSensorPin_${index}">${sensorConfig ? sensorConfig.sensorPin : 'N/A'}</td>
                <td class="input-cell"><div class="input-wrapper"><input type="number" id="sensorPin_${index}" min="0" max="40" value="${sensorConfig ? sensorConfig.sensorPin : ''}"></div></td>
            </tr>
            <tr>
                <td>Relay Pin</td>
                <td id="currentRelayPin_${index}">${sensorConfig ? sensorConfig.relayPin : 'N/A'}</td>
                <td class="input-cell"><div class="input-wrapper"><input type="number" id="relayPin_${index}" min="0" max="40" value="${sensorConfig ? sensorConfig.relayPin : ''}"></div></td>
            </tr>
        </table>
    `;
    return sensorDiv;
}

function createAndPopulateSensorConfigs(config) {
    console.log('Entering createAndPopulateSensorConfigs');
    console.log('Config:', JSON.stringify(config, null, 2));
    
    const sensorConfigsContainer = document.getElementById('sensorConfigs');
    sensorConfigsContainer.innerHTML = '';

    if (!config.sensorConfigs || !Array.isArray(config.sensorConfigs)) {
        console.error('sensorConfigs is missing or not an array:', config.sensorConfigs);
        return;
    }

    config.sensorConfigs.forEach((sensorConfig, index) => {
        console.log(`Processing sensor config ${index}:`, JSON.stringify(sensorConfig, null, 2));
        if (sensorConfig === null) {
            console.warn(`Sensor config at index ${index} is null, creating empty config`);
            sensorConfig = {
                threshold: 0,
                activationPeriod: 0,
                wateringInterval: 0,
                sensorEnabled: false,
                relayEnabled: false,
                sensorPin: 0,
                relayPin: 0
            };
        }
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
    console.log('Populating form with config:', JSON.stringify(config, null, 2));

    populateGlobalSettings(config);
    
    console.log('About to populate sensor configs');
    if (config.sensorConfigs && Array.isArray(config.sensorConfigs)) {
        console.log(`Found ${config.sensorConfigs.length} sensor configs`);
        createAndPopulateSensorConfigs(config);
    } else {
        console.error('sensorConfigs is missing or not an array:', config.sensorConfigs);
    }
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