# Orthodox Reminder Pro v5

Windows desktop aplikacija u C++ za pravoslavni kalendar i podsetnike.

## Sta ima u ovoj verziji
- futuristicki tamni interfejs
- custom mesecni kalendar
- datumi sa podsetnicima su obelezeni direktno u kalendaru
- podsetnici po datumu sa ponavljanjem
- pretraga, backup, restore i eksport izvestaja
- prosirena offline baza praznika i svetaca preko `data/orthodox_calendar_sr.csv`
- pokretni praznici se racunaju za svaku godinu

## Kako radi baza kalendara
Program ima:
- ugradjene glavne praznike, postove i velike slave
- dodatni offline CSV fajl koji se automatski ucitava iz foldera `data`

To znaci da gotov EXE radi bez interneta, ali za jos vecu bazu mozes kasnije samo da prosiris CSV bez menjanja koda.

## GitHub build
1. Uploaduj sadrzaj projekta na GitHub repo.
2. Idi na **Actions**.
3. Pokreni **Build Windows EXE**.
4. Preuzmi artifact `OrthodoxReminderPro-v5-windows`.

## Vazno
Za pravih 100% kompletan dnevni crkveni kalendar za svaku godinu treba dalje puniti i proveravati bazu podataka. Ova verzija je pripremljena tako da se ta baza lako siri offline, bez menjanja interfejsa i glavne logike programa.
