const $ = id => document.getElementById(id);
const money = n => `KSh ${Number(n).toFixed(2)}`;
function toast(message){const t=$('toast');t.textContent=message;t.classList.add('show');setTimeout(()=>t.classList.remove('show'),2800)}
function showResult(el, html, ok=true){el.innerHTML=html;el.className=`result ${ok?'success':'error'}`}
async function api(url, options={}){const r=await fetch(url,options);const data=await r.json();if(!r.ok)throw data;return data}
async function refresh(){
  try{
    const s=await api('/api/status');
    $('totalSlots').textContent=s.totalSlots;$('availableSlots').textContent=s.availableSlots;$('occupiedSlots').textContent=s.occupiedSlots;$('revenue').textContent=money(s.revenue);
    $('slotGrid').innerHTML=s.slots.map(x=>`<div class="slot ${x.occupied?'occupied':'available'}"><b>Slot ${String(x.id).padStart(2,'0')}</b><small>${x.occupied?x.plate:'Available'}</small></div>`).join('');
    const t=await api('/api/transactions');
    $('transactionBody').innerHTML=t.transactions.length?t.transactions.slice().reverse().map(x=>`<tr><td>${esc(x.plate)}</td><td>${x.slot}</td><td>${x.arrival}</td><td>${x.departure}</td><td>${x.durationMinutes} min</td><td>${money(x.fee)}</td><td>${x.paymentStatus}</td></tr>`).join(''):`<tr><td colspan="7" class="empty">No completed transactions yet.</td></tr>`;
  }catch(e){toast(e.message||'Could not refresh the system.');}
}
function esc(v){return String(v).replace(/[&<>'"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;',"'":'&#39;','"':'&quot;'}[c]))}
$('arrivalForm').addEventListener('submit',async e=>{e.preventDefault();const body=new URLSearchParams({plate:$('arrivalPlate').value,owner:$('owner').value});try{const r=await api('/api/arrival',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});toast(`Vehicle admitted to Slot ${String(r.slotId).padStart(2,'0')}`);e.target.reset();refresh()}catch(e){toast(e.message||'Arrival failed.')}});
$('exitForm').addEventListener('submit',async e=>{e.preventDefault();const body=new URLSearchParams({plate:$('exitPlate').value});try{const r=await api('/api/exit',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});showResult($('exitResult'),`<b>Barrier OPEN</b><br>Duration: ${r.durationMinutes} minute(s)<br>Amount due: <b>${money(r.fee)}</b><br>Status: ${r.fee===0?'FREE':'PAID'}`);toast(r.message);e.target.reset();refresh()}catch(e){showResult($('exitResult'),e.message||'Exit failed.',false)}});
$('searchForm').addEventListener('submit',async e=>{e.preventDefault();try{const plate=encodeURIComponent($('searchPlate').value);const r=await api(`/api/search?plate=${plate}`);showResult($('searchResult'),`<b>Vehicle found</b><br>Allocated slot: <b>${String(r.slotId).padStart(2,'0')}</b><br>Arrival: ${r.arrival}`)}catch(e){showResult($('searchResult'),e.message||'Vehicle not found.',false)}});
$('refreshBtn').addEventListener('click',refresh);refresh();setInterval(refresh,3000);
