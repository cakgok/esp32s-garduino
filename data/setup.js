let currentConfig = {};

document.addEventListener('DOMContentLoaded', () => {
    const setupForm = document.getElementById('setupForm');
    const resetToDefault = document.getElementById('resetToDefault');
    const systemSize = document.getElementById('systemSize');
    const returnToDashboard = document.getElementById('returnToDashboard');

    if (setupForm) setupForm.addEventListener('submit', saveConfig);
    if (resetToDefault) resetToDefault.addEventListener('click', resetToDefaultConfig);
    if (systemSize) systemSize.addEventListener('change', updatePinInputs);
    if (returnToDashboard) returnToDashboard.addEventListener('click', () => window.location.href = 'index.html');

    fetchConfig();
});

async function fetchConfig() {
    try {
        const response = await fetch('/api/setup');
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

function populateForm(config) {
    const systemSize = document.getElementById('systemSize');
    const sdaPin = document.getElementById('sdaPin');
    const sclPin = document.getElementById('sclPin');
    const floatSwitchPin = document.getElementById('floatSwitchPin');
    const sensorPinsContainer = document.getElementById('sensorPinsContainer');
    const relayPinsContainer = document.getElementById('relayPinsContainer');

    // Set current values
    document.getElementById('currentSystemSize').textContent = config.systemSize;
    document.getElementById('currentSdaPin').textContent = config.sdaPin;
    document.getElementById('currentSclPin').textContent = config.sclPin;
    document.getElementById('currentFloatSwitchPin').textContent = config.floatSwitchPin;

    // Set input values
    if (systemSize) systemSize.value = config.systemSize;
    if (sdaPin) sdaPin.value = config.sdaPin;
    if (sclPin) sclPin.value = config.sclPin;
    if (floatSwitchPin) floatSwitchPin.value = config.floatSwitchPin;

    if (sensorPinsContainer) {
        sensorPinsContainer.innerHTML = '';
        config.sensorPins.forEach((pin, index) => addPinInput('sensorPinsContainer', pin, index));
    }

    if (relayPinsContainer) {
        relayPinsContainer.innerHTML = '';
        config.relayPins.forEach((pin, index) => addPinInput('relayPinsContainer', pin, index));
    }
}

function addPinInput(containerId, value = '', index) {
    const container = document.getElementById(containerId);
    if (!container) return;

    const inputGroup = document.createElement('div');
    inputGroup.className = 'pin-input';
    inputGroup.innerHTML = `
        <label for="${containerId}-${index}">Pin ${index + 1}:</label>
        <input type="number" id="${containerId}-${index}" value="${value}" min="0" required>
    `;
    container.appendChild(inputGroup);
}

function updatePinInputs() {
    const systemSize = document.getElementById('systemSize');
    const sensorPinsContainer = document.getElementById('sensorPinsContainer');
    const relayPinsContainer = document.getElementById('relayPinsContainer');

    if (!systemSize || !sensorPinsContainer || !relayPinsContainer) return;

    const size = parseInt(systemSize.value);

    updatePinContainer(sensorPinsContainer, 'sensorPinsContainer', size);
    updatePinContainer(relayPinsContainer, 'relayPinsContainer', size);
}

function updatePinContainer(container, containerId, size) {
    while (container.children.length < size) {
        addPinInput(containerId, '', container.children.length);
    }
    while (container.children.length > size) {
        container.lastChild.remove();
    }
}

async function saveConfig(event) {
    event.preventDefault();
    const newConfig = {
        systemSize: parseInt(document.getElementById('systemSize').value),
        sdaPin: parseInt(document.getElementById('sdaPin').value),
        sclPin: parseInt(document.getElementById('sclPin').value),
        floatSwitchPin: parseInt(document.getElementById('floatSwitchPin').value),
        sensorPins: Array.from(document.getElementById('sensorPinsContainer').children).map(child => parseInt(child.querySelector('input').value)),
        relayPins: Array.from(document.getElementById('relayPinsContainer').children).map(child => parseInt(child.querySelector('input').value))
    };

    try {
        const response = await fetch('/api/setup', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(newConfig),
        });

        if (!response.ok) {
            throw new Error('Failed to save configuration');
        }

        alert('Configuration saved successfully. The system will now restart.');
        // You can add additional logic here to handle the restart if needed
    } catch (error) {
        console.error('Error:', error);
        alert('Failed to save configuration. Please try again.');
    }
}

async function resetToDefaultConfig() {
    if (confirm('Are you sure you want to reset to default settings? This action cannot be undone.')) {
        try {
            const response = await fetch('/api/resetSetup', {
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