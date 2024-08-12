// Configuration
const RELAY_COUNT = 4; // Update this based on your actual relay count
const UPDATE_INTERVAL = 5000; // 5 seconds

// DOM Elements
const temperatureElement = document.getElementById('temperature');
const pressureElement = document.getElementById('pressure');
const tempOffsetInput = document.getElementById('temp-offset');
const configForm = document.getElementById('config-form');
const logListElement = document.getElementById('log-list');
const relayControlsElement = document.getElementById('relay-controls');

// Fetch and update functions
async function fetchSensorData() {
    try {
        const response = await fetch('/api/sensorData');
        const data = await response.json();
        updateSensorDisplay(data);
    } catch (error) {
        console.error('Error fetching sensor data:', error);
    }
}

function updateSensorDisplay(data) {
    temperatureElement.textContent = data.temperature.toFixed(2);
    pressureElement.textContent = data.pressure.toFixed(2);
}

async function fetchConfig() {
    try {
        const response = await fetch('/api/config');
        const config = await response.json();
        tempOffsetInput.value = config.temperatureOffset;
    } catch (error) {
        console.error('Error fetching config:', error);
    }
}

async function updateConfig(event) {
    event.preventDefault();
    const tempOffset = tempOffsetInput.value;
    try {
        const response = await fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: `temperatureOffset=${tempOffset}`
        });
        if (!response.ok) throw new Error('Failed to update config');
        alert('Configuration updated successfully');
        await fetchConfig(); // Refresh the config
    } catch (error) {
        console.error('Error updating config:', error);
        alert('Failed to update configuration');
    }
}

async function fetchLogs() {
    try {
        const response = await fetch('/api/logs');
        const htmlLogs = await response.text();
        logListElement.innerHTML = htmlLogs;
    } catch (error) {
        console.error('Error fetching logs:', error);
    }
}

async function toggleRelay(relay) {
    try {
        const response = await fetch('/api/relay', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: `relay=${relay}&state=toggle`
        });
        if (!response.ok) throw new Error('Failed to toggle relay');
    } catch (error) {
        console.error('Error toggling relay:', error);
    }
}

// Setup functions
function setupRelayControls() {
    for (let i = 0; i < RELAY_COUNT; i++) {
        const button = document.createElement('button');
        button.textContent = `Toggle Relay ${i}`;
        button.addEventListener('click', () => toggleRelay(i));
        relayControlsElement.appendChild(button);
    }
}

function setupEventListeners() {
    configForm.addEventListener('submit', updateConfig);
}

// Initialization
function init() {
    setupEventListeners();
    setupRelayControls();
    fetchConfig();
    fetchSensorData();
    fetchLogs();

    // Set up periodic updates
    setInterval(fetchSensorData, UPDATE_INTERVAL);
    setInterval(fetchLogs, UPDATE_INTERVAL * 2);
}

// Start the application when the DOM is fully loaded
document.addEventListener('DOMContentLoaded', init);