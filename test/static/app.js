// AI GENERATED

document.addEventListener('DOMContentLoaded', () => {
    const btn = document.getElementById('api-btn');
    const resultBox = document.getElementById('api-result');

    btn.addEventListener('click', async () => {
        resultBox.textContent = 'Sending response...';
        resultBox.style.color = '#ffcc00';

        try {
            const response = await fetch('/api/test');
            
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }

            const data = await response.json();
            
            resultBox.textContent = JSON.stringify(data);
            resultBox.style.color = '#39ff14'; 
        } catch (error) {
            resultBox.textContent = `Error: ${error.message}`;
            resultBox.style.color = '#ff3333';
        }
    });
});