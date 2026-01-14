// GridPulse Dashboard - Frontend JavaScript

// Use same origin to avoid CORS issues (proxy handles backend requests)
const API_BASE = '';

// State
let devices = [];
let alerts = [];
let selectedDevice = null;
let tempChart = null;
let humidityChart = null;
let batteryChart = null;
let currentUser = null;
let refreshInterval = null;

// ========== Authentication ==========

function getToken() {
    return localStorage.getItem('gridpulse_token');
}

function setToken(token) {
    localStorage.setItem('gridpulse_token', token);
}

function removeToken() {
    localStorage.removeItem('gridpulse_token');
}

function getAuthHeaders() {
    const token = getToken();
    return token ? { 'Authorization': `Bearer ${token}` } : {};
}

// Show auth screen
function showAuthScreen() {
    document.getElementById('authScreen').classList.remove('hidden');
    document.getElementById('mainApp').classList.add('hidden');
    if (refreshInterval) {
        clearInterval(refreshInterval);
        refreshInterval = null;
    }
}

// Show main app
function showMainApp() {
    document.getElementById('authScreen').classList.add('hidden');
    document.getElementById('mainApp').classList.remove('hidden');
    if (currentUser) {
        document.getElementById('currentUser').textContent = currentUser.username;
    }
    initializeDashboard();
}

// Tab switching
function showLoginTab() {
    document.getElementById('loginTab').classList.add('active');
    document.getElementById('registerTab').classList.remove('active');
    document.getElementById('loginForm').classList.remove('hidden');
    document.getElementById('registerForm').classList.add('hidden');
    document.getElementById('loginError').textContent = '';
    document.getElementById('registerError').textContent = '';
}

function showRegisterTab() {
    document.getElementById('loginTab').classList.remove('active');
    document.getElementById('registerTab').classList.add('active');
    document.getElementById('loginForm').classList.add('hidden');
    document.getElementById('registerForm').classList.remove('hidden');
    document.getElementById('loginError').textContent = '';
    document.getElementById('registerError').textContent = '';
}

// Handle login
async function handleLogin(e) {
    e.preventDefault();
    const errorEl = document.getElementById('loginError');
    errorEl.textContent = '';
    
    const username = document.getElementById('loginUsername').value;
    const password = document.getElementById('loginPassword').value;
    
    try {
        const res = await fetch(`${API_BASE}/api/auth/login`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ username, password })
        });
        
        const data = await res.json();
        
        if (res.ok) {
            setToken(data.token);
            currentUser = data.user;
            showMainApp();
        } else {
            errorEl.textContent = data.error || 'Login failed';
        }
    } catch (e) {
        errorEl.textContent = 'Connection error. Please try again.';
    }
}

// Handle registration
async function handleRegister(e) {
    e.preventDefault();
    const errorEl = document.getElementById('registerError');
    errorEl.textContent = '';
    
    const username = document.getElementById('registerUsername').value;
    const email = document.getElementById('registerEmail').value;
    const password = document.getElementById('registerPassword').value;
    
    try {
        const res = await fetch(`${API_BASE}/api/auth/register`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ username, email, password })
        });
        
        const data = await res.json();
        
        if (res.ok) {
            setToken(data.token);
            currentUser = data.user;
            showMainApp();
        } else {
            errorEl.textContent = data.error || 'Registration failed';
        }
    } catch (e) {
        errorEl.textContent = 'Connection error. Please try again.';
    }
}

// Handle logout
function handleLogout() {
    removeToken();
    currentUser = null;
    selectedDevice = null;
    devices = [];
    alerts = [];
    showAuthScreen();
}

// Verify existing token
async function verifyToken() {
    const token = getToken();
    if (!token) {
        return false;
    }
    
    try {
        const res = await fetch(`${API_BASE}/api/auth/me`, {
            headers: getAuthHeaders()
        });
        
        if (res.ok) {
            currentUser = await res.json();
            return true;
        } else {
            removeToken();
            return false;
        }
    } catch (e) {
        return false;
    }
}

// Initialize dashboard after login
function initializeDashboard() {
    checkServerStatus();
    loadDevices();
    loadAlerts();
    
    // Auto-refresh every 1 second
    if (refreshInterval) clearInterval(refreshInterval);
    refreshInterval = setInterval(() => {
        loadDevices();
        loadAlerts();
        if (selectedDevice) {
            loadTelemetryChart();
        }
    }, 1000);
}

// Initialize app
document.addEventListener('DOMContentLoaded', async () => {
    // Check if user is already logged in
    const isLoggedIn = await verifyToken();
    
    if (isLoggedIn) {
        showMainApp();
    } else {
        showAuthScreen();
    }
});

// Server Status
async function checkServerStatus() {
    try {
        const res = await fetch(`${API_BASE}/health`);
        const data = await res.json();
        
        document.getElementById('serverStatus').className = 'status-dot online';
        document.getElementById('serverStatusText').textContent = `Server Online (v${data.version})`;
    } catch (e) {
        document.getElementById('serverStatus').className = 'status-dot offline';
        document.getElementById('serverStatusText').textContent = 'Server Offline';
    }
}

// Load Devices
async function loadDevices() {
    try {
        const res = await fetch(`${API_BASE}/api/devices`);
        const data = await res.json();
        devices = data.devices || [];
        
        renderDevices();
        updateStats();
        updateDeviceSelect();
    } catch (e) {
        console.error('Failed to load devices:', e);
    }
}

function renderDevices() {
    const container = document.getElementById('devicesList');
    
    if (devices.length === 0) {
        container.innerHTML = '<div class="loading">No devices registered yet.<br>Click "+ Add Device" to get started.</div>';
        return;
    }
    
    container.innerHTML = devices.map(device => `
        <div class="device-card ${selectedDevice === device.device_id ? 'selected' : ''}" 
             onclick="selectDevice('${device.device_id}')">
            <button class="device-delete" onclick="event.stopPropagation(); deleteDevice('${device.device_id}', '${device.name}')" title="Delete device">×</button>
            <div class="device-header">
                <span class="device-name">${device.name}</span>
                <span class="device-status ${device.status}">${device.status}</span>
            </div>
            <div class="device-id">${device.device_id}</div>
            <div class="device-location">📍 ${device.location || 'No location'}</div>
            <div class="device-actions">
                <button class="btn-telemetry" onclick="event.stopPropagation(); showTelemetry('${device.device_id}')">
                    📊 Send Data
                </button>
            </div>
        </div>
    `).join('');
}

function selectDevice(deviceId) {
    selectedDevice = deviceId;
    document.getElementById('chartDevice').value = deviceId;
    renderDevices();
    loadTelemetryChart();
}

async function deleteDevice(deviceId, deviceName) {
    if (!confirm(`Are you sure you want to delete "${deviceName}"?\n\nThis will permanently remove the device and all its telemetry data.`)) {
        return;
    }
    
    try {
        const res = await fetch(`${API_BASE}/api/delete-device?id=${encodeURIComponent(deviceId)}`, {
            method: 'POST'
        });
        
        if (res.ok) {
            if (selectedDevice === deviceId) {
                selectedDevice = null;
            }
            loadDevices();
        } else {
            const data = await res.json();
            alert(data.error || 'Failed to delete device');
        }
    } catch (e) {
        console.error('Failed to delete device:', e);
        alert('Failed to delete device');
    }
}

function updateDeviceSelect() {
    const select = document.getElementById('chartDevice');
    const currentValue = select.value;
    
    select.innerHTML = '<option value="">Select device...</option>' + 
        devices.map(d => `<option value="${d.device_id}">${d.name}</option>`).join('');
    
    if (currentValue) {
        select.value = currentValue;
    }
}

// Load Alerts
async function loadAlerts() {
    try {
        const res = await fetch(`${API_BASE}/api/alerts`);
        const data = await res.json();
        alerts = data.alerts || [];
        
        renderAlerts();
        document.getElementById('alertCount').textContent = alerts.length;
        document.getElementById('activeAlerts').textContent = alerts.length;
    } catch (e) {
        console.error('Failed to load alerts:', e);
    }
}

function renderAlerts() {
    const container = document.getElementById('alertsList');
    
    if (alerts.length === 0) {
        container.innerHTML = '<div class="no-alerts">✓ No active alerts<br><small>All systems normal</small></div>';
        return;
    }
    
    container.innerHTML = alerts.map(alert => `
        <div class="alert-card ${alert.severity}">
            <div class="alert-header">
                <span class="alert-type">${formatAlertType(alert.alert_type)}</span>
                <span class="alert-severity ${alert.severity}">${alert.severity}</span>
            </div>
            <div class="alert-message">${alert.message}</div>
            <div class="alert-device">${alert.device_id}</div>
            <div class="alert-actions">
                <button class="btn-ack" onclick="acknowledgeAlert(${alert.id})">Acknowledge</button>
            </div>
        </div>
    `).join('');
}

function formatAlertType(type) {
    const types = {
        'high_temperature': '🌡️ High Temperature',
        'low_battery': '🔋 Low Battery',
        'offline': '📡 Device Offline'
    };
    return types[type] || type;
}

async function acknowledgeAlert(alertId) {
    try {
        await fetch(`${API_BASE}/api/alerts/${alertId}/ack`, { method: 'POST' });
        loadAlerts();
    } catch (e) {
        console.error('Failed to acknowledge alert:', e);
    }
}

// Stats
function updateStats() {
    document.getElementById('totalDevices').textContent = devices.length;
    document.getElementById('onlineDevices').textContent = devices.filter(d => d.status === 'online').length;
}

// Telemetry Charts
async function loadTelemetryChart() {
    const deviceId = document.getElementById('chartDevice').value;
    
    if (!deviceId) {
        clearCharts();
        return;
    }
    
    try {
        const res = await fetch(`${API_BASE}/api/telemetry?device_id=${encodeURIComponent(deviceId)}&limit=20`);
        const data = await res.json();
        const telemetry = (data.telemetry || []).reverse();
        
        if (telemetry.length === 0) {
            clearCharts();
            return;
        }
        
        const labels = telemetry.map((t, i) => `#${i + 1}`);
        const temps = telemetry.map(t => t.temperature);
        const humidities = telemetry.map(t => t.humidity);
        const batteries = telemetry.map(t => t.battery_level);
        
        // Update average temp stat
        const avgTemp = temps.reduce((a, b) => a + b, 0) / temps.length;
        document.getElementById('avgTemp').textContent = avgTemp.toFixed(1) + '°C';
        
        updateTempChart(labels, temps);
        updateHumidityChart(labels, humidities);
        updateBatteryChart(labels, batteries);
    } catch (e) {
        console.error('Failed to load telemetry:', e);
    }
}

function clearCharts() {
    if (tempChart) {
        tempChart.destroy();
        tempChart = null;
    }
    if (humidityChart) {
        humidityChart.destroy();
        humidityChart = null;
    }
    if (batteryChart) {
        batteryChart.destroy();
        batteryChart = null;
    }
}

function updateTempChart(labels, data) {
    const ctx = document.getElementById('tempChart').getContext('2d');
    
    if (tempChart) {
        tempChart.data.labels = labels;
        tempChart.data.datasets[0].data = data;
        tempChart.update();
    } else {
        tempChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels,
                datasets: [{
                    label: 'Temperature (°C)',
                    data,
                    borderColor: '#00d4aa',
                    backgroundColor: 'rgba(0, 212, 170, 0.1)',
                    fill: true,
                    tension: 0.4
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        labels: { color: '#888899' }
                    }
                },
                scales: {
                    x: {
                        ticks: { color: '#888899' },
                        grid: { color: '#2a2a3a' }
                    },
                    y: {
                        ticks: { color: '#888899' },
                        grid: { color: '#2a2a3a' }
                    }
                }
            }
        });
    }
}

function updateHumidityChart(labels, data) {
    const ctx = document.getElementById('humidityChart').getContext('2d');
    
    if (humidityChart) {
        humidityChart.data.labels = labels;
        humidityChart.data.datasets[0].data = data;
        humidityChart.update();
    } else {
        humidityChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels,
                datasets: [{
                    label: 'Humidity (%)',
                    data,
                    borderColor: '#3b82f6',
                    backgroundColor: 'rgba(59, 130, 246, 0.1)',
                    fill: true,
                    tension: 0.4
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        labels: { color: '#888899' }
                    }
                },
                scales: {
                    x: {
                        ticks: { color: '#888899' },
                        grid: { color: '#2a2a3a' }
                    },
                    y: {
                        ticks: { color: '#888899' },
                        grid: { color: '#2a2a3a' },
                        min: 0,
                        max: 100
                    }
                }
            }
        });
    }
}

function updateBatteryChart(labels, data) {
    const ctx = document.getElementById('batteryChart').getContext('2d');
    
    if (batteryChart) {
        batteryChart.data.labels = labels;
        batteryChart.data.datasets[0].data = data;
        batteryChart.update();
    } else {
        batteryChart = new Chart(ctx, {
            type: 'bar',
            data: {
                labels,
                datasets: [{
                    label: 'Battery (%)',
                    data,
                    backgroundColor: data.map(v => v < 20 ? '#ff4466' : v < 50 ? '#ffaa00' : '#00dd88'),
                    borderRadius: 4
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        labels: { color: '#888899' }
                    }
                },
                scales: {
                    x: {
                        ticks: { color: '#888899' },
                        grid: { color: '#2a2a3a' }
                    },
                    y: {
                        ticks: { color: '#888899' },
                        grid: { color: '#2a2a3a' },
                        max: 100
                    }
                }
            }
        });
    }
}

// Add Device Modal
function showAddDevice() {
    document.getElementById('addDeviceModal').classList.add('active');
}

function hideAddDevice() {
    document.getElementById('addDeviceModal').classList.remove('active');
    document.getElementById('addDeviceForm').reset();
}

async function addDevice(e) {
    e.preventDefault();
    
    const device = {
        device_id: document.getElementById('deviceId').value.replace(/\s+/g, '-'),
        name: document.getElementById('deviceName').value,
        location: document.getElementById('deviceLocation').value
    };
    
    try {
        const res = await fetch(`${API_BASE}/api/devices`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(device)
        });
        
        if (res.ok) {
            hideAddDevice();
            loadDevices();
        } else {
            const data = await res.json();
            alert(data.error || 'Failed to add device');
        }
    } catch (e) {
        alert('Failed to connect to server');
    }
}

// Send Telemetry Modal
function showTelemetry(deviceId) {
    document.getElementById('telemetryDeviceId').value = deviceId;
    document.getElementById('telemetryModal').classList.add('active');
}

function hideTelemetry() {
    document.getElementById('telemetryModal').classList.remove('active');
}

async function sendTelemetry(e) {
    e.preventDefault();
    
    const telemetry = {
        device_id: document.getElementById('telemetryDeviceId').value,
        temperature: parseFloat(document.getElementById('telemetryTemp').value),
        humidity: parseFloat(document.getElementById('telemetryHumidity').value),
        battery_level: parseFloat(document.getElementById('telemetryBattery').value)
    };
    
    try {
        const res = await fetch(`${API_BASE}/api/telemetry`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(telemetry)
        });
        
        if (res.ok) {
            hideTelemetry();
            loadDevices();
            loadAlerts();
            // Auto-select the device we just sent data to and refresh chart
            selectedDevice = telemetry.device_id;
            document.getElementById('chartDevice').value = telemetry.device_id;
            loadTelemetryChart();
        } else {
            const data = await res.json();
            alert(data.error || 'Failed to send telemetry');
        }
    } catch (e) {
        alert('Failed to connect to server');
    }
}

// Close modals on outside click
document.querySelectorAll('.modal').forEach(modal => {
    modal.addEventListener('click', (e) => {
        if (e.target === modal) {
            modal.classList.remove('active');
        }
    });
});
