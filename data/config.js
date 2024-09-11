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
        { id: 'currentTelemetryInterval', value: formatDuration(config.telemetryInterval, 'seconds') },
        { id: 'currentSensorUpdateInterval', value: formatDuration(config.sensorUpdateInterval, 'seconds') },
        { id: 'currentLcdUpdateInterval', value: formatDuration(config.lcdUpdateInterval, 'seconds') },
        { id: 'currentSensorPublishInterval', value: formatDuration(config.sensorPublishInterval, 'seconds') },
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

function createSensorConfigHTML(sensorConfig, index) {
    console.log(`Creating HTML for sensor config ${index}:`, sensorConfig);
    const sensorDiv = document.createElement('div');
    sensorDiv.className = `sensor-config ${sensorConfig.sensorEnabled ? '' : 'disabled'}`;
    sensorDiv.innerHTML = `
        <h3>Sensor ${index + 1}</h3>
        <div class="checkbox-group">
            <div class="checkbox-wrapper">
                <input type="checkbox" id="sensorEnabled_${index}" ${sensorConfig.sensorEnabled ? 'checked' : ''}>
                <label for="sensorEnabled_${index}">Enable Sensor</label>
            </div>
            <div class="checkbox-wrapper">
                <input type="checkbox" id="relayEnabled_${index}" ${sensorConfig.relayEnabled ? 'checked' : ''}>
                <label for="relayEnabled_${index}">Enable Relay Control</label>
            </div>
        </div>
        <div class="sensor-details">
            <table>
                <tr>
                    <th>Setting</th>
                    <th>Current Value</th>
                    <th>New Value</th>
                </tr>
                <tr>
                    <td>Threshold</td>
                    <td id="currentThreshold_${index}">${sensorConfig.threshold.toFixed(1)}</td>
                    <td class="input-cell"><div class="input-wrapper"><input type="number" id="threshold_${index}" step="0.1" min="5" max="50" value="${sensorConfig.threshold}"></div></td>
                </tr>
                <tr>
                    <td>Activation Period (s)</td>
                    <td id="currentActivationPeriod_${index}">${formatDuration(sensorConfig.activationPeriod, 'seconds')}</td>
                    <td class="input-cell"><div class="input-wrapper"><input type="number" id="activationPeriod_${index}" step="1" min="1" max="60" value="${sensorConfig.activationPeriod / 1000}"></div></td>
                </tr>
                <tr>
                    <td>Watering Interval (h)</td>
                    <td id="currentWateringInterval_${index}">${formatDuration(sensorConfig.wateringInterval, 'hours')}</td>
                    <td class="input-cell"><div class="input-wrapper"><input type="number" id="wateringInterval_${index}" step="1" min="1" max="120" value="${sensorConfig.wateringInterval / 3600000}"></div></td>
                </tr>
            </table>
        </div>
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
                const sensorDetails = sensorDiv.querySelector('.sensor-details');
                const relayCheckbox = document.getElementById(`relayEnabled_${index}`);
                
                if (input.checked) {
                    sensorDiv.classList.remove('disabled');
                    sensorDetails.style.pointerEvents = 'auto';
                    sensorDetails.style.opacity = '1';
                    relayCheckbox.disabled = false;
                } else {
                    sensorDiv.classList.add('disabled');
                    sensorDetails.style.pointerEvents = 'none';
                    sensorDetails.style.opacity = '0.5';
                    relayCheckbox.disabled = true;
                    relayCheckbox.checked = false;
                }
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

function formatDuration(ms, unit) {
    switch (unit) {
        case 'seconds':
            return `${(ms / 1000).toFixed(1)} seconds`;
        case 'minutes':
            return `${(ms / 60000).toFixed(1)} minutes`;
        case 'hours':
            return `${(ms / 3600000).toFixed(1)} hours`;
        default:
            return `${ms} ms`;
    }
}

function updateSaveButton() {
    const saveButton = document.getElementById('saveConfig');
    saveButton.classList.toggle('active', hasChanges);
}

async function saveConfig() {
    const newConfig = {
        temperatureOffset: parseFloat(document.getElementById('tempOffset').value),
        telemetryInterval: parseInt(document.getElementById('telemetryInterval').value) * 1000,
        sensorUpdateInterval: parseFloat(document.getElementById('sensorUpdateInterval').value) * 1000,
        lcdUpdateInterval: parseFloat(document.getElementById('lcdUpdateInterval').value) * 1000,
        sensorPublishInterval: parseInt(document.getElementById('sensorPublishInterval').value) * 1000,
        sensorConfigs: currentConfig.sensorConfigs.map((_, index) => ({
            threshold: parseFloat(document.getElementById(`threshold_${index}`).value),
            activationPeriod: parseInt(document.getElementById(`activationPeriod_${index}`).value) * 1000,
            wateringInterval: parseFloat(document.getElementById(`wateringInterval_${index}`).value) * 3600000,
            sensorEnabled: document.getElementById(`sensorEnabled_${index}`).checked,
            relayEnabled: document.getElementById(`relayEnabled_${index}`).checked,
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