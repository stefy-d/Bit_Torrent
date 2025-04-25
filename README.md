# Bit_Torrent

<br>

## **```Descrierea metodei de rezolvare```**

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Pentru început pe măsură ce am citit fișierul de intrare al fiecărui client am reținut aceste
date într-o structură pentru fiecare client *(clients)* dar am și trimis tracker-ului informațiile
despre fișiere. După ce tracker-ul termină de primit un fișier, verifică dacă a mai primit acest fișier.
Dacă este unul nou, îl adaugă la lista de fișiere pe care o construiește *(all_files)*. De asemenea, adaugă clientul de la care a primit date ca seed al fișierului. 

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Apoi, după ce tracker-ul termină de primit de la toți clienții, am decis să trimit de la
acesta către clienți lista cu toate fișierele pe care tocmai a construit-o. Fac acest lucru la
început pentru că atunci când începe descărcarea fiecare client trebuie să știe ce segment cere/primește și câte sunt în total pentru fiecare fișier.

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Trimit *ack* clienților, iar aceștia deschid cele două thread-uri: download și upload.

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;În **download** am ales să descarc pe rând fiecare fișier, iar segmentele acestuia în ordine. Astfel, atunci
când încep descărcarea unui nou fișier cer swarm-ul acestuia de la tracker. Tot acum, 
tracker-ul adaugă clientul ca peer al fișierului respectiv. Tracker-ul îmi trimite ce se află în
swarm și pun aceste informații într-o coadă. După ce primesc swarm-ul și alcătuiesc coada, 
apelez funcția *shuffle_queue* care îmi randomizează ordinea elementelor din coadă. Fac acest lucru 
pentru că swarm-ul ținut la tracker este făcut din două set-uri(seeds și peers), iar asta înseamnă că fiind 
ordonate, elementele din swarm-ul aceluiași fișier vor ajunge în aceeași ordine la doi clienți care
să zicem că vor să descarce același fișier în același timp. Așa că, randomizând ordinea clienților din swarm
atunci când îl primesc contribuie la partea de eficiență pentru că pentru același fișier 
descărcarea de mai mulți clienți se va încerca într-o ordine diferită.

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;După ce am obținut swarm-ul, descarc segmentele în ordine astfel că știu să mă opresc atunci când
am un număr de segmente egal cu numărul de segmente al fișierului pe care îl știu din lista cu
toate fișierele primită de la tracker la inițializare. Astfel, cât timp nu am toate segmentele,
scot din coadă primul element, mă asigur că nu m-am extras chiar pe mine, pentru că evident că nu
am cum să cer segmentul de la mine, rețin peer-ul de la care vreau să cer și îl adaug la finalul cozii.
Această parcurgere circulară a swarm-ului îmi permite să variez clienții de la care se încearcă 
descărcarea. 

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Peer-ului ales din coadă îi zic că vreau să fac o descărcare și îi trimit 
numele fișierului și indexul segmentului pentru a putea face verificarea dacă are sau nu.
Aștept să primesc un răspuns de la acesta, astfel că dacă primesc „1” înseamnă că l-a găsit
și pot incrementa numărul de segmente pe care le am din fișier, iar la următoarea iterație 
trec la descărcarea următorului segment. Dacă primesc un răspuns negativ, încerc din nou 
să descarc același segment de la următorul peer.

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Atunci când am obținut toate segmentele, anunț tracker-ul de acest lucru, tracker-ul marchează 
clientul ca seed al fișierului. De asemenea, clientul scrie în fișierul de ieșire hash-urile
fișierului pe care tocmai l-a descărcat.
După ce clientul termină de descărcat toate fișierele anunță tracker-ul care îi trimite permisiunea
să închidă thread-ul download.

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;În **upload** cât este deschis, pot să primesc mesaj de la tracker, ca să închid thread-ul 
sau de la client ca să verific dacă am un anumit segment dintr-un fișier. Dacă am segmentul trimit
clientului „1”, iar dacă nu îi trimit „0”. Atunci când primesc mesajul de la tracker, închid thread-ul.

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;**Tracker-ul**, prin variabila *active_clients* ține evidența clienților care mai au de descărcat fișiere.
El este anunțat când un client termină toate descărcările și în acel moment decrementează
*active_clients*. Când variabila ajunge la 0 anunță clienții să-și închidă și thread-urile de upload.

<br>

## **```Eficiența```**

> &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Pentru ca un peer al unui fișier să nu fie supraîncărcat, la cererea swarm-ului randomizez 
ordinea în care sunt seeds și peers deoarece vreau ca fiecae client să încerce descărcarea într-o altă ordine.
De asemenea, faptul că utilizez o coadă îmi permite ca odată ce am folosit un peer să îl
mut la final. Acest lucru face ca toți clienții să fie folosiți aproximativ egal, 
evitând supraîncărcarea unui singur peer și ajutând la diversificarea clienților de la care se face descărcarea.

<br>

## **```Etichete și Tag-uri```**

### *Etichete*: sunt folosite pentru a-i trimite tracker-ului tipul solicitării din partea clientului; de asemenea, le folosește și thread-ul upload pentru a decide ce are de făcut.

+ **REQUEST:** Un client cere detalii despre swarm-ul unui fișier.
+ **FILE_COMPLETED**: Un client informează tracker-ul că a descărcat complet un fișier.
+ **DOWNLOAD_ALL_DONE:** Clientul semnalează că a terminat descărcarea tuturor fișierelor dorite.
+ **UPLOAD:** Gestionează încărcarea fișierelor între clienți.
+ **CLOSE_ALL:** Semnalizează tuturor clienților să își închidă thread-urile upload.

### *Tag-uri*: sunt folosite pentru a mă asigura că mesajele sunt trimise și primite corespunzător.

+ **TAG_SEND_INFO:** Trimite datele fișierelor de la clienți către tracker.
+ **TAG_ALL_FILES:** Tracker-ul trimite toate detaliile despre fișiere de la tracker către clienți.
+ **TAG_ACK:** Confirmare din partea tracker-ului pentru a începe descărcările.
+ **TAG_TYPE:** Indică că s-a trimis un mesaj cu tipul acțiunii.
+ **TAG_SWARM**: Utilizat când se trimit informații despre swarm-ul unui fișier.
+ **TAG_CLOSE_DOWNLOAD:** Semnal de închidere pentru thread-ul download.
+ **TAG_UPLOAD:** Indică cererile de încărcare între clienți.
+ **TAG_FILE_COMPLETED:** Confirmare a finalizării descărcării unui fișier.
+ **TAG_CONFIRMATION:** Confirmare de la un client că un segment a fost găsit sau nu.

<br>

## **```Funcții principale```**

### ***<u>tracker</u>*** 
Pentru fiecare task, trecker-ul primește informațiile despre ce fișiere are 
    task-ul respectiv și marchează ca seed clientul de la care primește datele. 
    De asemenea, tracker-ul reține în vectorul *all_files* ce hash-uri are fiecare fișier.

După ce colectează fișierele de la clienți, tracker-ul le trimite tuturor 
    clienților informațiile complete despre fișiere. 
    
Apoi, tracker-ul trimite *ack* clienților pentru a le indica că au 
    permisiunea de a începe descărcarea.

Tracker-ul ciclează într-un *while* cât timp există încă clienți care nu au 
    terminat de descărcat tot ce trebuie. Astfel, tracker-ul la fiecare iterație 
    primește întâi un mesaj care indică ce acțiune are de făcut:
    
> **REQUEST:** indică faptul că un client a solicitat swarm-ul unui fișier. 
        Primește apoi și numele fișierului și apelează funcția *send_swarm*.

> **FILE_COMPLETED:** indică faptul că un client a terminat de descărcat un fișier. 
        Tracker-ul marchează clientul ca seed al fișierului și îl elimină din set-ul peers.

> **DOWNLOAD_ALL_DONE:** indică faptul că un client a terminat de descărcat tot ce trebuia. Tracker-ul decrementează numărul de clienți activi și îi trimite 
        clientului mesaj să-ți închidă thread-ul de download.

Când variabila *active_clients* ajunge la 0 înseamnă că toți clienții au terminat 
de descărcat tot. Se iese din while și tracker-ul trimite tuturor clienților 
mesaj că pot să închidă și thread-urile upload.

<br>

### ***<u>peer</u>***
Funcția *peer*, apelează *read_files* ca să citească fișierul de intrare al fiecărui client 
    în parte și *get_data* pentru a prelua de la tracker datele despre toate fișierele care există.

Apoi, clientul așteaptă să primească permisiune de la tracker ca să înceapă descărcarea 
    fișierelor. După ce primește permisiunea, clientul deschide thread-urile download și upload.

<br>

### ***<u>upload_thread_func</u>***
Este funcția thread-ului upload în care se ciclează într-un *while*.

Când există un mesaj care este pentru acest client, primește tipul acțiunii 
    pe care o are de făcut. În upload se primesc mesaje și de la alți clienți
    dar și de la tracker:

>**UPLOAD:** indică faptul că există o cerere de descărcare a unui hash care aparține 
    unui fișier al cărui seed/peer este clientul. Se verifică dacă are hash-ul și 
    trimite înapoi clientului care a cerut, 1 dacă s-a găsit hash-ul sau 0 dacă nu îl are.

>**CLOSE_ALL:** indică faptul că tracker-ul a trimis mesaj de închidere a thread-ului upload.

<br>

### ***<u>download_thread_func</u>***
Funcția thread-ului download în care descarc pentru fiecare fișier 
    pe rând hash-urile în ordine.

Când încep descărcarea unui fișier, cer de la tracker swarm-ul prin funcția 
    *request_swarm* și rețin într-o coadă ce peers și seeds primesc.

Pentru că am zis că descarc în ordine hash-urile ciclez într-un while cât timp numărul 
    hash-urilor pe care le am din fișier este mai mic decât câte hash-uri are fișierul în total.

În acest *while*, la fiecare 10 hash-uri descărcate cer de la tracker o actualizare a swarm-ului. 
    Swarm-ul este reținut într-o coadă din care scot primul element și îl adaug la loc în coadă la 
    sfârșit. Dacă peer-ul extras este chiar clientul care vrea să facă descărcarea mai extrag o 
    dată din coadă pentru a nu încerca să descarce hash-ul de la el însuși. 

După ce a fost selectat un peer de la care să se încerce descărcarea, îi trimite 
    acestuia fișierul din care vrea să descarce și indexul hash-ului. Când clientul 
    primește răspuns de la peer verific dacă am primit „1” pentru a putea trece la 
    următorul hash. Dacă s-a primit un răspuns negativ, în următoarea iterație a 
    buclei se încearcă descărcarea aceluiași hash.

Atunci când s-au strâns toate hash-urile, clientul trimite la tracker mesaj 
    că fișierul este complet. Clientul creează fișierul de ieșire în care pune 
    hash-urile fișierului descărcat.

După ce au fost descărcate toate fișierele dorite, clientul îi trimite mesaj 
    tracker-ului că a terminat descărcarea. Primește de la tracker mesaj că 
    poate închide thread-ul download.

<br>

## **```Funcții auxiliare```**

### ***read_files***
> Se ocupă cu citirea fișierelor de intrare și reține fișierele pe care le 
are fiecare client și pe cele pe care trebuie să le descarce. De asemenea, pe măsură 
ce sunt citite fișierele de intrare, sunt trimise datele și la tracker.

### ***get_data*** 
>Funcție prin care fiecare client primește de la tracker informațiile 
despre fiecare fișier care există.

### ***send_swarm*** 
>Această funcție trimite swarm-ul fișierului dorit către clientul care 
l-a solicitat. De asemenea, setează ca peer clientul dacă acesta nu este găsit printre seeds.

### ***shuffle_queue*** 
>Randomizează ordinea clienților din swarm-ul primit de la tracker 
la apelul funcției *request_swarm* a unui client.

### ***request_swarm*** 
>Cere de la tracker swarm-ul unui fișier, amestecă elementele din swarm 
pentru a nu fi aceeași ordine la toți clienții care descarcă simultan acest fișier 
și returnează clientului elementele într-o coadă.

<br>
