// Application configuration
const CONFIG = {
    API_STATUS: '/api/status',
    API_RECORDING_START: '/api/recording/start',
    API_RECORDING_STOP: '/api/recording/stop',
    API_FILES: '/api/files',
    WS_ENDPOINT: 'ws://' + window.location.hostname + ':' + window.location.port + '/ws/live',
    STATUS_UPDATE_INTERVAL: 1000,  // 1 second
    CHART_MAX_POINTS: 150,          // ~5 seconds at 30Hz
};

// Application state
const state = {
    isRecording: false,
    timeSynced: false,
    wsConnected: false,
    ws: null,
    chart: null,
    chartDataRaw: [],
    chartDataProc: [],
    reconnectAttempts: 0,
    maxReconnectAttempts: 10,
    reconnectDelay: 1000,
};

// ============================================================================
// Initialization
// ============================================================================

document.addEventListener('DOMContentLoaded', () => {
    initializeChart();
    setupEventListeners();
    connectWebSocket();
    pollStatus();
    loadFilesList();

    // Display device IP
    fetch('/api/status')
        .then(r => r.json())
        .then(data => {
            document.getElementById('deviceIP').textContent = window.location.hostname;
            document.getElementById('hostinfo').textContent = `Connected at ${new Date().toLocaleTimeString()}`;
        });

    // Periodic status updates
    setInterval(pollStatus, CONFIG.STATUS_UPDATE_INTERVAL);

    // Reload file list every 10 seconds
    setInterval(loadFilesList, 10000);
});

// ============================================================================
// Chart Initialization
// ============================================================================

function initializeChart() {
    const ctx = document.getElementById('ecgChart').getContext('2d');

    state.chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                {
                    label: 'ECG Raw',
                    data: [],
                    borderColor: '#007bff',
                    backgroundColor: 'rgba(0, 123, 255, 0.05)',
                    borderWidth: 2,
                    pointRadius: 0,
                    pointHoverRadius: 4,
                    tension: 0.1,
                    fill: false,
                },
                {
                    label: 'ECG Processed',
                    data: [],
                    borderColor: '#dc3545',
                    backgroundColor: 'rgba(220, 53, 69, 0.05)',
                    borderWidth: 2,
                    pointRadius: 0,
                    pointHoverRadius: 4,
                    tension: 0.1,
                    fill: false,
                },
            ],
        },
        options: {
            responsive: true,
            maintainAspectRatio: true,
            animation: {
                duration: 0,  // Disable animations for real-time performance
            },
            plugins: {
                legend: {
                    display: true,
                    position: 'top',
                },
            },
            scales: {
                x: {
                    display: true,
                    type: 'linear',
                    min: 0,
                    max: CONFIG.CHART_MAX_POINTS,
                },
                y: {
                    display: true,
                    ticks: {
                        beginAtZero: false,
                    },
                },
            },
        },
    });
}

// ============================================================================
// WebSocket Connection
// ============================================================================

function connectWebSocket() {
    try {
        state.ws = new WebSocket(CONFIG.WS_ENDPOINT);

        state.ws.onopen = () => {
            console.log('WebSocket connected');
            state.wsConnected = true;
            state.reconnectAttempts = 0;
            updateConnectionStatus();
        };

        state.ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                updateChartWithSample(data);
            } catch (e) {
                console.error('Failed to parse WebSocket message:', e);
            }
        };

        state.ws.onerror = (error) => {
            console.error('WebSocket error:', error);
        };

        state.ws.onclose = () => {
            console.log('WebSocket closed');
            state.wsConnected = false;
            updateConnectionStatus();
            attemptReconnect();
        };
    } catch (error) {
        console.error('Failed to create WebSocket:', error);
        attemptReconnect();
    }
}

function attemptReconnect() {
    if (state.reconnectAttempts < state.maxReconnectAttempts) {
        state.reconnectAttempts++;
        const delay = state.reconnectDelay * Math.pow(2, state.reconnectAttempts - 1);
        console.log(`Attempting to reconnect in ${delay}ms (attempt ${state.reconnectAttempts}/${state.maxReconnectAttempts})`);
        setTimeout(connectWebSocket, delay);
    } else {
        console.error('Max WebSocket reconnection attempts reached');
    }
}

function updateConnectionStatus() {
    const hostinfo = document.getElementById('hostinfo');
    if (state.wsConnected) {
        hostinfo.textContent = '🟢 Connected | ' + new Date().toLocaleTimeString();
        hostinfo.style.color = '#28a745';
    } else {
        hostinfo.textContent = '🔴 Disconnected | Attempting to reconnect...';
        hostinfo.style.color = '#dc3545';
    }
}

// ============================================================================
// Chart Updates
// ============================================================================

function updateChartWithSample(sample) {
    if (!state.chart) return;

    state.chartDataRaw.push(sample.ecg_raw || 0);
    state.chartDataProc.push(sample.ecg_proc || 0);

    // Keep only the last CHART_MAX_POINTS
    if (state.chartDataRaw.length > CONFIG.CHART_MAX_POINTS) {
        state.chartDataRaw.shift();
        state.chartDataProc.shift();
    }

    // Update chart
    state.chart.data.labels = Array.from({length: state.chartDataRaw.length}, (_, i) => i);
    state.chart.data.datasets[0].data = state.chartDataRaw;
    state.chart.data.datasets[1].data = state.chartDataProc;
    state.chart.update('none');  // Update without animation

    // Update last ECG value display
    document.getElementById('lastEcgValue').textContent = (sample.ecg_raw || 0).toString();
}

// ============================================================================
// Event Listeners
// ============================================================================

function setupEventListeners() {
    const startBtn = document.getElementById('startBtn');
    const stopBtn = document.getElementById('stopBtn');
    const form = document.getElementById('metadataForm');

    startBtn.addEventListener('click', () => {
        startRecording();
    });

    stopBtn.addEventListener('click', () => {
        stopRecording();
    });

    // Prevent form submission on Enter
    form.addEventListener('submit', (e) => {
        e.preventDefault();
    });
}

// ============================================================================
// Recording Control
// ============================================================================

async function startRecording() {
    const filename = document.getElementById('filename').value.trim();
    const dogId = document.getElementById('dogId').value.trim();
    const experimentId = document.getElementById('experimentId').value.trim();

    try {
        const response = await fetch(CONFIG.API_RECORDING_START, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                filename: filename,
                dog_id: dogId,
                experiment_id: experimentId,
            }),
        });

        if (response.ok) {
            state.isRecording = true;
            updateUIState();
            console.log('Recording started');
            showNotification('Recording started!', 'success');
        } else {
            showNotification('Failed to start recording', 'error');
        }
    } catch (error) {
        console.error('Error starting recording:', error);
        showNotification('Error: ' + error.message, 'error');
    }
}

async function stopRecording() {
    try {
        const response = await fetch(CONFIG.API_RECORDING_STOP, {
            method: 'POST',
        });

        if (response.ok) {
            state.isRecording = false;
            updateUIState();
            console.log('Recording stopped');
            showNotification('Recording stopped!', 'success');
            
            // Reload file list after recording stops
            setTimeout(loadFilesList, 500);
        } else {
            showNotification('Failed to stop recording', 'error');
        }
    } catch (error) {
        console.error('Error stopping recording:', error);
        showNotification('Error: ' + error.message, 'error');
    }
}

// ============================================================================
// Status Polling
// ============================================================================

async function pollStatus() {
    try {
        const response = await fetch(CONFIG.API_STATUS);
        if (!response.ok) return;

        const data = await response.json();

        // Update recording state
        if (data.recording !== state.isRecording) {
            state.isRecording = data.recording;
            updateUIState();
        }

        // Update time sync status
        if (data.time_synced !== state.timeSynced) {
            state.timeSynced = data.time_synced;
            updateTimeSyncStatus();
        }

        // Update last ECG value if no WebSocket data
        if (data.last_sample && !state.wsConnected) {
            document.getElementById('lastEcgValue').textContent = data.last_sample.ecg_raw.toString();
        }

        // Update recording status display
        updateRecordingStatus();
    } catch (error) {
        console.error('Error polling status:', error);
    }
}

// ============================================================================
// UI Updates
// ============================================================================

function updateUIState() {
    const startBtn = document.getElementById('startBtn');
    const stopBtn = document.getElementById('stopBtn');
    const metadataForm = document.getElementById('metadataForm');

    if (state.isRecording) {
        startBtn.disabled = true;
        stopBtn.disabled = false;
        metadataForm.classList.add('recording-indicator');
    } else {
        startBtn.disabled = false;
        stopBtn.disabled = true;
        metadataForm.classList.remove('recording-indicator');
    }

    updateRecordingStatus();
}

function updateRecordingStatus() {
    const status = document.getElementById('recordingStatus');
    if (state.isRecording) {
        status.textContent = '🔴 Recording';
        status.style.color = '#dc3545';
    } else {
        status.textContent = '⏹️ Standby';
        status.style.color = '#6c757d';
    }
}

function updateTimeSyncStatus() {
    const status = document.getElementById('timeSyncedStatus');
    if (state.timeSynced) {
        status.textContent = '✅ Synced';
        status.style.color = '#28a745';
    } else {
        status.textContent = '❌ Not Synced';
        status.style.color = '#dc3545';
    }
}

// ============================================================================
// Files Management
// ============================================================================

async function loadFilesList() {
    try {
        const response = await fetch(CONFIG.API_FILES);
        if (!response.ok) return;

        const data = await response.json();
        const filesList = document.getElementById('filesList');

        if (!data.files || data.files.length === 0) {
            filesList.innerHTML = '<p class="loading">No recordings yet</p>';
            return;
        }

        const sortedFiles = [...data.files].sort((a, b) => (b.time || 0) - (a.time || 0));

        let html = '';
        sortedFiles.forEach(file => {
            const sizeKB = (file.size / 1024).toFixed(1);
            const fileDate = new Date(file.time * 1000).toLocaleString();

            html += `
                <div class="file-item">
                    <div class="file-info">
                        <span class="file-name">${escapeHtml(file.name)}</span>
                        <span class="file-size">${sizeKB} KB | ${fileDate}</span>
                    </div>
                    <div class="file-actions">
                        <button class="btn-small btn-download" onclick="downloadFile('${escapeHtml(file.name)}')">
                            Download
                        </button>
                    </div>
                </div>
            `;
        });

        filesList.innerHTML = html;
    } catch (error) {
        console.error('Error loading files list:', error);
        document.getElementById('filesList').innerHTML = '<p class="error">Failed to load files list</p>';
    }
}

function downloadFile(filename) {
    const link = document.createElement('a');
    link.href = `/download?file=${encodeURIComponent(filename)}`;
    link.download = filename;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
}

// ============================================================================
// Notifications
// ============================================================================

function showNotification(message, type = 'info') {
    const div = document.createElement('div');
    div.className = type;
    div.textContent = message;
    div.style.position = 'fixed';
    div.style.top = '20px';
    div.style.right = '20px';
    div.style.zIndex = '10000';
    div.style.maxWidth = '400px';
    div.style.animation = 'slideIn 0.3s ease-in-out';

    document.body.appendChild(div);

    setTimeout(() => {
        div.style.animation = 'slideOut 0.3s ease-in-out';
        setTimeout(() => document.body.removeChild(div), 300);
    }, 3000);
}

// ============================================================================
// Utilities
// ============================================================================

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// Add animation styles
const style = document.createElement('style');
style.textContent = `
    @keyframes slideIn {
        from {
            transform: translateX(400px);
            opacity: 0;
        }
        to {
            transform: translateX(0);
            opacity: 1;
        }
    }

    @keyframes slideOut {
        from {
            transform: translateX(0);
            opacity: 1;
        }
        to {
            transform: translateX(400px);
            opacity: 0;
        }
    }
`;
document.head.appendChild(style);
