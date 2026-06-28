# Using LLM for the Design and Compiler Implementation

## Very Shortly about Me

I was born in 1975 (in Hungary). I'm writing computer programs since 1988 (approx.). I sold my first software in 1994 (written in FoxPro). Since 1999 software development is my main profession too. Until 2026 I used more than 10 different programming languages (counting only those, where I had at least a half-year project).

Before I started with the DQ I already had several years experience creating text parsers, mainly for special configuration files. Some of them even had expression evaluators. I developed a lot in Assembly, for different architectures, but I did not have experience translating expressions into machine code or register allocations.

## Motivation

* Python runtime errors
* Missing Delphi / FreePascal
* C++ evilness
* Shrinking Pascal Community
* Problems with FreePascal: case sensitivity, module system, std library


Megnéztem sok új (hype-olt) nyelvet:
* Zig
* Swift
* Rust
* Mojo

Sajnos mindegyikkel volt valami problémám.


## The First LLM Sessions

Az elégedetlenségem a meglevő nyelvekkel kapcsolatban folyamatosan foglalkoztatott, egy idő után már azon kezdtem gondolkozni, hogy milyen lenne az ideális programnyelv, ha én tervezném. 2025 év végén vásároltam egy ChatGPT Plus előfizetést, ami lehetővé tette a folyamatos hozzáférést intelligens modellhez. Ezzel a modellel meglepően jól meg tudtam vitatni az elméleti kérdéseimet a programnyelv tervezésével kapcsolatban. Először szigorúan csak chat-session-ben használtam.

A DQ nyelv tényleges tervezése 2026. Jan. 18-án kezdődött. Az első prompt így nézett ki:

```
I would like to design a new programming language.
I already know well pascal, python, C++, JavaScript (among other not so generic languages like VHDL, FoxPro, ActionScript, SQL).

What I don't like in C, C++:
- implicit type conversions: 3 / 2 * 10 != 3 * 10 / 2, or masked_value = (value & (1 < 2) ), no explicit boolean
- terrible operator precedence
- not readable syntax (* for pointers and multiplication, function pointer definition etc)

What I don't like in Python:
- not compiled, gives a lot of runtime errors
- object access require self.
- does not fully supports all object oriented features
- way of handling imports
- no explicit boolean

What I don't like in Pascal:
- Case insensitive (later this is rather a problem)
- Handling of var parameters sometimes
- inconsistent run-time library
- weak debugger
- too much heap usage

I was searching for a long time for some utility language, which is generic and stable. I don't like the syntax of Rust, the others are either not supporting object inheritance or have the same C roots with the [3 / 2 * 10 != 3 * 10 / 2] problem.

Is it a good idea to design a new language?
```
A ChatGPT 5.3 nem támogatta először az ötletemet, először például még meg kellett győznöm az OOP fontosságáról, aztán belálátta, hogy ilyen nyelv tényleg nem létezik, ill. nem talált ilyet.

A kérdés feltevésekor még egyáltalán nem volt szándékomban a nyelvet implementálni is, de a válaszokban már ott voltak az implementációval kapcsolatos fogalmak is, mint például as LLVM.

Aztán folytatódtak a megbeszélések, már konkrét forráskód példákkal.

Tudtam, hogy a mesterséges intelligenciában nem szabad vakon megbízni, de rendkívül hasznos volt, hogy egyfajta másodvélemény, a kapcsolódó elmélet és gyakorlati megvalósítások is villámgyorsan elérhetőek voltak. Láttam, hogy meglevő mintákat jól tud keresni és prezentálni, de a kreativitása viszonylag korlátozottabb. Ezt a néhány esetben hiányzó kreativitást én pótoltam, néha elég erőszakosnak kellett lennem, hogy megértse és elfogadja a nem szokványos megoldásaimat.

## Using a Real LLM Agent for the First Time

A hosszú chat-session-ök kezdtek körülményessé válni, láttam több helyen, hogy profik editorba integrált LLM-et használnak több, ill. nagyobb fájlok szerkesztéséhez. A megnövekedett DQ specifikációjának a kezelésére nekem is erre volt szükségem. Próbáltam beállítani a VSCode-ot Codex Pluginnal, de akkor még nem működött a ChatGPT Plus előfizetéssel. Ezért vásároltam egy claude.ai pro plan-t. Ezzel meglepően gyorsan ment a specifikáció véglegesítése, módosítása. Vegyesen használtam a Claude-ot és a ChatGPT-t, tesztelgettem őket. A ChatGPT előfizetést fel is mondtam, de még egy hónapig tudtam használni.

Az alap specifikáció elkészült, gondoltam letesztelem a Claude-ot és utasítottam, hogy a specifikáció alapján készítsen egy egyszerű compilert, amit egyelőre csak C testbed-ből lehet használni. Nagy meglepetésemre, kb 30 perc alatt elkészült és működött. A kód struktúrált volt valamennyire, de nem túl ideális módon. Volt több kisebb fájl és egy nagy (codegen), ebben volt a legtöbb logika. Aminek a legjobban örültem, hogy benne voltak a leglényegesebb LLVM hívások.

## Designing the Proper Compiler

Világossá vált számomra, hogy AI segítsággel sokkal rövidebb idő alatt el tudok készíteni egy megfelelő DQ compilert.
Láttam, hogy az alapvető architektúra tervezésénél sokat kell segítenem neki, de az LLVM kapcsolatot elég jól tudja managelni, amit nem is igazán akartam megtanulni.

A Claude által generált compilert egy darabig megtartottam, mert kicsi volt, és gondoltam, azt úgy jobban tudja kezelni, és látni akartam még, hogyan egészíti ki további funkciókkal. Párhuzamosan el kezdtem tervezni az igazi DQ compilert. Ehhez vásároltam még egy Google Gemini Pro előfizetést is. 2026 január végére egyszerre három aktív AI előfizetésem volt: ChatGPT Plus, claude.ai pro és Gemini Pro. Teszteltem mindhármat mindenféle feladatokra. Mindíg csak a lemagasabb intelligenciájú modelleket használtam. Az a kép alakult ki bennem, hogy a chat felületen a ChatGPT-5.3-5.4 ill. Gemini 3.1 Pro sokkal hasznosabbak voltak a Claude-nál. A Claude többnyire csak egy megoldást prezentált, kevés indoklással. A legtöbb szöveget mindíg a ChatGPT produkálta, amit néha fáradságos volt már végigolvasni. A ChatGPT és Gemini alternatíva keresésésnél például más kevésbé ismert megoldásokat prezentált.

A forráskód értelmezéséhez az általam jól ismert és bevált technikát szerettem volna használni, ezért az általam korábban fejlesztett TStrParseObj-ot némileg átalakítottam, és ebből lett a DQ source code feeder-e (scf). Ezzel a technikával nincs szökség különálló tokenizer-re és preprocessorra (az scf->SkipWhite() feldolgozza a compiler direktívákat is). A compiler direktívák feldolgozását (#ifdef, #include stb.) saját kezűleg írtam meg, csak pár egyszerűbb feladatot hagytam későbbre.

A legfőbb objektum architektúrákat kézzel készítettem elő (OValSym, OType, OScope etc.) ezzel is mutattam az AI-nak, hogy milyen stílusban szeretnék dolgozni. Ezután a kompilert kisebb lépésekben az claude agenttel fokozatosan építettem. A workflow tehát az volt, hogy elméleti kérdéseket a Gemini Pro-ban vagy ChatGPT-ben tisztáztam, aztán az implementáció főleg Claude-al ment, előbb implementációs tervet kértem attól is, finomítottam rajta, ha kellett, azán hagytam dolgozni.

Párszor próbáltam a Gemini-t agent módban a VSCode-ból, de nagyon körülményes volt, hemzsegett a hibáktól, úgyhogy a Gemini Pro előfizetésemet hamarosan fel is mondtam.

## Keeping Up with the AI

Észrevettem, hogy a fejleszés ezen a módon sokkal gyorsabban halad, de ennek megfelelően sokkal több kód is keletkezik. Megtanultam, hogy az AI által generált kódot mindíg át kell nézni. Jegyzeteket készítettem a kevésbé elegáns, esetleg később problémássá válható megoldásokról. Néha kézzel igazítottam meg, de aztán láttam, hogy AI maga is ki tudja javítani ezeket a tökéletlenségeket.

Elég megeröltető volt tehát a sok AI kódot átnézni, de fontos, hogy ismerjem a kódot és kezelhető maradjon. Az AI kód/architektúrák minőségével alapvetően meg voltam elégedve. Azt nem is gondoltam volna, hogy teszeket is tud írni ill. debuggolni.

## DQ Auto-Test

Hamarosan látszott, hogy szükség van egy komolyabb compiler tesztelő rendszerre. Olyan megodások nem érdekeltek, amik Python-t használtak. Tudtam, hogy később nagyon nagy mennyiségű, kissebb fájlokat kell feldolgozni ezért inkább saját, C++-ban írt egyszerű rendszer mellett döntöttem. Fontos volt a párhuzamos feldolgozás. Ez AI segítséggel már nem volt annyira nehéz feladat. Végül a teljes projekt tovább tartott, mert a kimeneti formátumokat meg kellett tervezni, és bizonyos részeit kézzel készítettem elő.

A DQ compiler fejleszésének nagy lökést adott az automatikus teszt rendszer. Az AI is jól tudta használni, képes volt új teszteket készíteni hozzá. Fontos lépés volt a printf() függvény beintegrálása, és a C-segédkód nélküli önállóan futó DQ programok lehetőségének megteremtése.

## Reaching Rate Limits with the Claude

Ahogy a fordító növekedett, az AI agent egyre lassabban dolgozott vele, egyre többször elértem már az 5-órás rate-limit-et, az AI munkát tervezni kellett a rate limit-ekhez, de ennek ellenére sem akartam a kevésbé intelligens modellt használni, mert a megoldások minősége így sem volt mindig a legjobb.

Megpróbáltam mégegyszer a ChatGPT Codex plugin-jét, és ezúttal már sikerült beállítanom a VSCode-ban a ChatGPT Plus előfizetéssel is. Azt tapasztaltam, hogy Claude-hoz képest sokkal kevésbé fogyasztja a keretet és sokkal gyorsabban végez egy-egy feladattal, ill. nem szükséges jóváhagyásokat adni neki, sokkal önállóbban dolgozik. Ekkor átváltottam teljesen ChatGPT Codex-re.

