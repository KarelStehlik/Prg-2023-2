# a simple fairy chess engine
příkaz help vypíše to samé anglicky, což více vyhovuje konzoli. pro úplnost  jsem vložil i český překlad:

## [obecně]
Ahoj!
V této aplikaci lze hrát šachy, možno i s vymyšlenými figurkami a/nebo proti počítači.
Hraje se do té doby, co někdo vezme krále.
Možné příkazy jsou: piece, fen, reset, takeback, toggleBlackBot, toggleWhiteBot, help.
Když vstup nezačíná příkazem, program se ho pokusí interpretovat jako tah.

## [move]
Tah se hraje tak, že napíšete počáteční a cílové pole, např. e2e4 jako první tah.

## [piece]
Vytvoří novou figurku.
očekává identifikátor figurky (a-z), max. jeden speciální argument a popis tahů.

### Speciální argumenty jsou:
- mirror <xy>: možné pohyby jsou duplikovány s odzrcadlenými X a/nebo Y souřadnicemi.
Příklad, střelec: piece b mirror xy 11>22>33>44>55>66>77
- symmetric: možné pohyby jsou duplikovány s otočením o 90, 180 a 270 stupňů
Příklad, kůň: piece n symmetric 12,21
- combine: neočekává popis tahů, ale seznam existujících figurek, které má kombinovat. Vytvoří kopie, takže následné změny těchto figurek nejsou přeneseny.
Příklad, královna: piece q combine r,b

### Popis tahů:
Obsahuje řadu pozic, symboly "," a ">". Pozice "11" znamená pohyb o 1 doprava nahoru. Popis "11, -1-1, 1-1, -11" znamená o 1 políčko šikmo v jakémkoliv směru, a "10>20" znamená že se může pohnout doprava o 1, pak pokračovat doprava o 2 pokud první políčko bylo prázdné. V některých případech jsou možné závorky, "symmetric 11>(21,12)" je něco jako kůň který neskáče. Ovšem není možné udělat tah vycházející ze závorky, jako "(10,01)>11".

### Pozici v popisu může následovat nějaká kombinace vlastností:
c - musí brát
n - nemůže brát
=X - povýší se na X pokud skončí na poslední řadě. V době zadávání musí X existovat, a pozdější změny k X ovlivní i to, na co se tato figurka povýší.
f - možno zahrát pouze pokud se figurka zatím nepohnula
všechny tyto vlastnosti kombinuje pěšec:
piece p 01n=q>02nf,11c=q,-11c=q


## [fen]
Bez argumentů vypíše FEN momentální pozice
S argumentem se pokusí načíst zadaný FEN.
Rošáda a en passant nejsou podporovány, takže odpovídající pole ve FENu jsou ignorovány.

## [reset]
Načte výchozí pozici, zachová definice figurek. je ekvivalentní s "fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 0"

## [takeback]
Vezme zpátky poslední tah (pro oba hráče)

## [toggleBlackBot]
Změní, zda za černého (zobrazen červeně) hraje počítač, výchozí stav je ano.

## [toggleWhiteBot]
Změní, zda za bílého (zobrazen zeleně) hraje počítač, výchozí stav je ne.

## [help]
Bez argumentů vypíše celý help soubor.
S argumentem se pokusí najít a vypsat zadanou [kategorii].
