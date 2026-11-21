Deze lijkt af en toe te werken:
fetch('/override/downshift', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: 'request=1' }).then(r => r.text()).then(console.log).catch(console.error);
Kan het zijn dat andere fetches ervoor zorgen dat deze op een gegeven moment niet meer werkt? Zodra er ergens een error verschijnt werkt deze ook niet meer

Ja! Sterker nog, ik denk dat we nu een belangrijke oorzaak te pakken hebben.
Ik vermoed dat je ESP32WebServer verzadigd raakt door gelijktijdige of achtergebleven HTTP-verbindingen.
Dat past bij jouw situatie:
    Je hebt een pollingfunctie die iedere 500 ms requests verstuurt.
    De ESP32WebServer kan maar één client tegelijk verwerken.
    Mobiele browsers kunnen keep-alive-verbindingen openhouden.
    Zodra een request vastloopt, kunnen volgende requests ook mislukken.
    Je Downshift-request werkt soms wel en soms niet.

Er is nog een belangrijk detail: handleClient() moet de tijd krijgen om iedere request af te handelen. Als je hoofdloop ondertussen veel ander werk doet, kan dat ook problemen veroorzaken.
1. Mijn eerste advies: voorkom overlappende fetches

Als je polling iedere 500 ms uitvoert, maar een request duurt langer dan 500 ms, kunnen requests zich opstapelen.

Gebruik bijvoorbeeld een eenvoudige lock:
javascript

let statusRequestInProgress = false;

function pollStatus() {
    if (statusRequestInProgress) return;

    statusRequestInProgress = true;

    fetch('/status')
        .then(r => r.json())
        .then(data => {
            // Verwerk status
        })
        .catch(error => {
            console.error('Status polling:', error);
        })
        .finally(() => {
            statusRequestInProgress = false;
        });
}

Hiermee voorkom je dat dezelfde pollingrequest meerdere keren tegelijk loopt.
2. Gebruik een aparte lock voor shift-requests

Voorkom ook dat iemand meerdere keren achter elkaar op Downshift of Upshift drukt terwijl de vorige request nog bezig is:
javascript

let shiftRequestInProgress = false;

function sendGearLeverOverrideShift(which) {
    if (shiftRequestInProgress) return;

    shiftRequestInProgress = true;

    fetch('/override/' + which, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'request=1'
    })
    .then(r => {
        if (!r.ok) throw new Error('HTTP ' + r.status);
        return r.text();
    })
    .then(console.log)
    .catch(console.error)
    .finally(() => {
        shiftRequestInProgress = false;
    });
}

3. Nog beter: alle requests centraal beheren

Omdat de ESP32 maar één client tegelijk verwerkt, zou ik uiteindelijk één centrale request-lock gebruiken voor alle fetches.

Bijvoorbeeld:
javascript

let espRequestBusy = false;

async function espFetch(url, options = {}) {
    while (espRequestBusy) {
        await new Promise(resolve => setTimeout(resolve, 25));
    }

    espRequestBusy = true;

    try {
        return await fetch(url, options);
    } finally {
        espRequestBusy = false;
    }
}

Je shift-functie wordt dan:
javascript

async function sendGearLeverOverrideShift(which) {
    try {
        const r = await espFetch('/override/' + which, {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: 'request=1'
        });

        if (!r.ok) alert(await r.text());

    } catch (e) {
        console.error('Shift request failed:', e);
        alert('Shift request failed: ' + e);
    }
}

Gebruik espFetch() vervolgens ook voor je status-polling en alle andere HTTP-requests naar de ESP32.

Let op: bij een langdurig vastgelopen request blijft de lock bezet. Een timeout kan dat voorkomen, maar een afgebroken browserrequest garandeert niet dat de ESP32 de verwerking ook heeft gestopt.
4. Eén belangrijk punt over jouw ESP32WebServer

Je had eerder deze code:
cpp

webServer.send(code, contentType, content);
webServer.client().stop();

Je commentaar over HTTP_MAX_CLOSE_WAIT is relevant. Als je de client niet sluit, kan de server tijd verliezen aan het afhandelen van openstaande keep-alive-verbindingen.

Ik zou daarom ook willen kijken naar de volledige pollingimplementatie en de versie van de library voordat we definitief besluiten hoe we de TCP-verbindingen moeten afhandelen.
Wat ik nu graag wil zien

Kun je jouw JavaScript-code van de 500ms status-polling laten zien?

Dat is op dit moment mijn belangrijkste verdachte. Ik wil vooral controleren of:

    Een nieuwe fetch gestart wordt voordat de vorige klaar is.

    Er meerdere pollingfuncties tegelijk actief zijn.

    Er requests blijven lopen na een netwerkfout.

Ik denk dat we dit kunnen oplossen zonder je ESP32-webserver te vervangen.
