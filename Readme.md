Router-ul are instructiuni intr-o bucla infinita, care dicteaza cum trebuie sa dirijeze sau sa raspunda la fiecare pachet primit.
Inainte de bucla, sunt cateva actiuni care trebuie realizate:
    - alocarea tabelei de rutare
    - alocarea tabelei arp
    - crearea si initializarea trie-ului pentru cautarea eficienta in tabela de rutare
    - initializarea cozii pentru pachetele arp care trebuie sa astepte

Trie-ul este contruit prin inserarea nodurilor cu informatia intrarea din tabela de rutare, in locul corespunzator. Trie-ul e parcurs bit cu bit folosind bitii mastii, iar intrarea este salvata in nodul in care s-a ajuns (in cazul in care un nod din parcurgere nu exista, acesta este creat).

Prima instructiune din bucla este extragerea informatiilor din ethernet frame prin utilizarea ether header-ului. Apoi, se ramifica in functie de ether_type.

Daca este 0x0800, atunci urmeaza sa extragem header-ul ip-ului al pachetului IPv4. In continuare, trebuie sa verificam daca pachetul este destinat router-ului. Se ia ip-ul destinatie din ip_hdr->daddr si este convertit in char*, pentru a putea fi comparat cu get_interface_ip(interface), unde interface este indicele interfetei de unde pachetul a fost primit.
    -  Daca destinatia este router-ul, se trimite un pachet ICMP cu type = 0, adica un reply. Acesta este construit prin inversarea adreselor mac sursa si destinatie din ether header, respectiv adreselor ip din ip header. Se recalculeaza checksum-ul in icmp header si in ip header. Apoi pachetul este trimit pe interfata de pe care a fost primit.
    - Daca destinatia nu este router-ul, pachetul trebuie dirijat catre destinatie de catre acesta. Mai intai, se recalculeaza checksum-ul din ip header si se compara cu cel vechi pentru a determina daca este gresit sau nu.
        - Daca este gresit, pachetul se arunca.  Altfel, se verifica daca ttl-ul este 0 sau 1. Daca da, pachetul este aruncat, iar router-ul trimite un nou pachet icmp "Time exceeded", type = 3. Acesta este contruit de la 0, copiind informatii folositoare din headerele eth si ip ale vechiului pachet, asemanator cu cel descris inainte.
        - Altfel, ttl-ul este decrementat si checksum-ul din ip header este recalculat.
    In continuare, se face cautarea in tabela de rutare pentru a gasi urmatorul hop din cea mai buna ruta pana la destinatie, in functie de ip_hdr->daddr. Daca nu este gasita nicio ruta, router-ul trimite un mesaj ICMP "Destination unreachable", type = 11. Altfel, se ia adresa mac a interfetei pe care va trebui scos pachetul (interfata este in intrarea din tabela de rutare returnata) si se copiaza ca adresa sursa din ether header. Apoi, se ia adresa mac corespunzatoare adresei ip a next hop-ului, care devine destinatia din ether header, iar pachetul este trimis pe interfata catre urmatorul hop.
    Adresele mac corespunzatoare adreselor ip sunt retinute in tabela arp. Pentru ca implementarea ei este dinamica, trebuie verificat daca exista corespondenta dintre ip si mac de fiecare data cand facem o cautare in ea. Astfel, daca in procedeul de mai sus, nu am gasi o intrare in tabela, pachetul curent va fi adaugat intr-o coada de pachete, pana cand va putea fi redirectionat. Apoi, router-ul va construi un pachet arp request, pentru a afla adresa mac a ip-ului din next hop. Adresa mac destinatie din ether header va fi cea de broadcast ff:ff:ff:ff:ff:ff. Sunt initializate toate campurile din arp header si este trimis pachetul de pe interfata catre next hop.


Daca este 0x806, vom prelucra un pachet arp. Se extrage header-ul acestuia si apoi se prelucreaza diferit in functie de tipul sau: request(1) sau reply(2).
    - Daca este un request, trebuie sa trimitem un arp reply cu adresa mac dorita. In arp header, adresa target devine adresa sursa, iar adresa sursa devine mac-ul interfetei de pe care a fost primit pachetul. Se inverseaza ip-urile in arp_hdr->spa si arp_hdr->tpa. In ether header, adresa destinatie devine adresa sursa, iar adresa sursa este adresa sursa din arp header. Apoi este trimis pachetul
    - Daca este un reply, se pune in tabela arp o noua intrare cu ip-ul si mac-ul cele sursa din arp header. Apoi, trebuie parcursa coada si trimise pachetele pentru care este posibil. La fiecare pas, se extrage un element din coada si informatiile din el, ether header si ip header. Se calculeaza best router si apoi se cauta ip-ul in arp. Daca nu este gasit, pachetul este adaugat din nou in coada. Daca este gasit, se trimite pachetul pe interfata urmatoare, cu mac-ul destinatie cel din tabela si mac-ul sursa cel al interfetei, in ether header.
