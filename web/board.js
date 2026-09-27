const grid = document.getElementById('slotGrid');
const escapeHtml = value => String(value).replace(/[&<>"']/g, character => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[character]));

async function refreshBoard() {
  const status = document.getElementById('boardStatus');
  try {
    const response = await fetch('/api/status', {cache: 'no-store'});
    if (!response.ok) throw new Error('Parking system unavailable');
    const data = await response.json();
    document.getElementById('availableCount').textContent = data.availableSlots;
    document.getElementById('occupiedCount').textContent = data.occupiedSlots;
    grid.innerHTML = data.slots.map(slot => `<article class="bay ${slot.occupied ? 'full' : ''}"><strong>Bay ${String(slot.id).padStart(2, '0')}</strong><span>${slot.occupied ? `Occupied · ${escapeHtml(slot.plate)}` : 'Available'}</span></article>`).join('');
    document.getElementById('updatedAt').textContent = new Date().toLocaleTimeString();
    status.textContent = 'Live availability';
  } catch (error) {
    status.textContent = error.message;
  }
}

refreshBoard();
setInterval(refreshBoard, 3000);