document.addEventListener('DOMContentLoaded', () => {
    
    // --- Power Toggle ---
    const powerToggle = document.getElementById('power-toggle');
    const powerLed = document.getElementById('power-led');
    const powerStatusText = document.getElementById('power-status-text');

    function updatePower() {
        if (powerToggle.checked) {
            powerLed.classList.add('active');
            powerStatusText.textContent = 'ON';
            powerStatusText.style.color = 'var(--accent-orange)';
        } else {
            powerLed.classList.remove('active');
            powerStatusText.textContent = 'OFF';
            powerStatusText.style.color = '#555';
        }
    }
    
    powerToggle.addEventListener('change', updatePower);
    updatePower(); // init


    // --- Size Selector ---
    const sizeSelector = document.getElementById('size-selector');
    const pluginContainer = document.getElementById('plugin-container');

    sizeSelector.addEventListener('change', (e) => {
        // Remove all size classes
        pluginContainer.classList.remove('size-small', 'size-medium', 'size-large');
        // Add selected
        pluginContainer.classList.add(e.target.value);
    });


    // --- Knob Logic ---
    const knobDial = document.getElementById('knob-dial');
    const semitoneValue = document.getElementById('semitone-value');
    const tickMarks = document.getElementById('tick-marks');
    
    const MIN_VALUE = -24;
    const MAX_VALUE = 24;
    const MIN_ANGLE = -135;
    const MAX_ANGLE = 135;
    const ANGLE_RANGE = MAX_ANGLE - MIN_ANGLE;
    const VALUE_RANGE = MAX_VALUE - MIN_VALUE;

    let currentValue = 0;
    
    // Draw tick marks
    function drawTicks() {
        const radius = 80; // distance from center
        const totalTicks = 49; // -24 to +24 is 49 values
        
        for (let i = 0; i < totalTicks; i++) {
            const val = MIN_VALUE + i;
            const isMajor = val % 6 === 0;
            
            // Calculate angle for this tick
            const percentage = i / (totalTicks - 1);
            const angle = MIN_ANGLE + (percentage * ANGLE_RANGE);
            
            const tick = document.createElement('div');
            tick.classList.add('tick');
            if (isMajor) tick.classList.add('major');
            
            // Rotate the tick
            tick.style.transform = `translateX(-50%) rotate(${angle}deg)`;
            tickMarks.appendChild(tick);

            // Add label for major ticks
            if (isMajor) {
                const label = document.createElement('div');
                label.classList.add('tick-label');
                label.textContent = val > 0 ? `+${val}` : val;
                
                // Position label
                const labelAngleRad = (angle - 90) * (Math.PI / 180);
                const labelRadius = radius + 25;
                const x = 90 + Math.cos(labelAngleRad) * labelRadius;
                const y = 90 + Math.sin(labelAngleRad) * labelRadius;
                
                label.style.left = `${x}px`;
                label.style.top = `${y}px`;
                tickMarks.appendChild(label);
            }
        }
    }
    drawTicks();

    // Set initial rotation
    function updateKnobDisplay() {
        const percentage = (currentValue - MIN_VALUE) / VALUE_RANGE;
        const angle = MIN_ANGLE + (percentage * ANGLE_RANGE);
        knobDial.style.transform = `rotate(${angle}deg)`;
        semitoneValue.textContent = currentValue > 0 ? `+${currentValue}` : currentValue;
    }
    updateKnobDisplay();

    // Drag interaction
    let isDragging = false;
    let startY = 0;
    let startValue = 0;

    knobDial.addEventListener('mousedown', (e) => {
        isDragging = true;
        startY = e.clientY;
        startValue = currentValue;
        document.body.style.cursor = 'grabbing';
    });

    document.addEventListener('mousemove', (e) => {
        if (!isDragging) return;
        
        const deltaY = startY - e.clientY;
        // Sensitivity: 5 pixels per value unit
        const valueChange = Math.round(deltaY / 3); 
        
        currentValue = startValue + valueChange;
        
        // Clamp
        if (currentValue < MIN_VALUE) currentValue = MIN_VALUE;
        if (currentValue > MAX_VALUE) currentValue = MAX_VALUE;
        
        updateKnobDisplay();
    });

    document.addEventListener('mouseup', () => {
        isDragging = false;
        document.body.style.cursor = 'default';
    });

});
