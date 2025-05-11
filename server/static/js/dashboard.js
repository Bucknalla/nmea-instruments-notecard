// Function to create compass markings
function createCompassMarkings(containerId) {
    const container = document.getElementById(containerId);
    for (let i = 0; i < 360; i += 5) {
        const marking = document.createElement('div');
        marking.className = 'compass-marking' + (i % 30 === 0 ? ' major' : '');
        marking.style.transform = `rotate(${i}deg)`;
        container.appendChild(marking);
    }
}

// Initialize compass markings when the page loads
document.addEventListener('DOMContentLoaded', function() {
    createCompassMarkings('heading-markings');
    createCompassMarkings('wind-markings');

    // Initialize map
    const map = L.map('map', {
        zoomControl: true,
        minZoom: 2,
        maxZoom: 18
    }).setView([50.3588825, -4.12234765625], 10);
    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        attribution: '© OpenStreetMap contributors'
    }).addTo(map);

    // Move zoom control to top right
    map.zoomControl.setPosition('topright');

    // Create custom boat icon using Font Awesome
    const boatIcon = L.divIcon({
        html: '<i class="fas fa-sailboat" style="color: #0066cc; font-size: 24px;"></i>',
        className: 'boat-icon',
        iconSize: [24, 24],
        iconAnchor: [12, 12]
    });

    // Create a marker that we'll update with the boat icon
    const marker = L.marker([0, 0], {
        icon: boatIcon,
        rotationAngle: 0,
        rotationOrigin: 'center center'
    }).addTo(map);

    // Store map and marker in window object for access in updateDisplay
    window.map = map;
    window.marker = marker;
});

// Function to update compass display
function updateCompass(compassId, angle) {
    const compass = document.getElementById(compassId);
    if (compass) {
        // Rotate the compass face in the opposite direction of the angle
        // This makes the compass face rotate counterclockwise as the angle increases
        compass.style.transform = `rotate(${-angle}deg)`;
    }
}

// Function to update the display with new data
function updateDisplay(data) {
    if (data.status === 'success' && data.parsed_data) {
        // Process HDV section
        if (data.parsed_data.HDV) {
            data.parsed_data.HDV.forEach(msg => {
                if (msg.type === 'HDG') {
                    document.getElementById('hdg-value').textContent = msg.data[0];
                    updateCompass('heading-compass', parseFloat(msg.data[0]));
                } else if (msg.type === 'HDM') {
                    document.getElementById('hdm-value').textContent = msg.data[0];
                }
            });
        }

        // Process INS section
        if (data.parsed_data.INS) {
            data.parsed_data.INS.forEach(msg => {
                if (msg.type === 'MWV') {
                    document.getElementById('wind-speed').textContent = msg.data[2];
                    let windAngle = parseFloat(msg.data[0]);
                    let displayAngle = windAngle;
                    let side = '';

                    if (windAngle > 180) {
                        displayAngle = 360 - windAngle;
                        side = ' (port)';
                    } else {
                        side = ' (starboard)';
                    }

                    document.getElementById('wind-angle').textContent = displayAngle.toFixed(1) + side;
                    updateCompass('wind-compass', windAngle);
                } else if (msg.type === 'DBT') {
                    document.getElementById('depth-dbt').textContent = msg.data[2];
                } else if (msg.type === 'DPT') {
                    document.getElementById('depth-dpt').textContent = msg.data[0];
                } else if (msg.type === 'DBK') {
                    document.getElementById('depth-dbk').textContent = msg.data[0];
                } else if (msg.type === 'MTW') {
                    document.getElementById('water-temp').textContent = msg.data[0];
                }
            });
        }

        // Update map if location data is available
        if (data.location) {
            const { latitude, longitude } = data.location;
            if (latitude && longitude) {
                const newLatLng = [latitude, longitude];
                window.marker.setLatLng(newLatLng);

                // Get current center and calculate distance
                const currentCenter = window.map.getCenter();
                const distance = currentCenter.distanceTo(newLatLng);

                // If this is the first update or the location has changed significantly (> 1000 meters)
                if (!window.lastLocation || distance > 1000) {
                    window.map.setView(newLatLng, 13);
                    window.lastLocation = newLatLng;
                }
            }
        }

        // Update raw data display
        document.getElementById('raw-data').textContent = JSON.stringify(data, null, 2);
    }
}

// Function to fetch the latest data
function fetchData() {
    fetch('/latest_data')
        .then(response => response.json())
        .then(data => {
            updateDisplay(data);
        })
        .catch(error => console.error('Error fetching data:', error));
}

// Fetch data every second
setInterval(fetchData, 1000);

// Initial fetch
fetchData();