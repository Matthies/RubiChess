ELO-Tests:
==========

Turniere gegen acht (halbwegs stabilen) Engines mit vergleichbarer, leicht höherer Spielstärke bei TC 40/10s bzw. 10s+0.1s, 200 Partien gegen jeden Gegner mit getauschten Positionen aus 2moves_v1

System: Intel(R) Xeon(R) CPU           E5620  @ 2.40GHz

ELOStat Anfangswert: 1956 (Mittelwert der verwendeten Engines aus der CCRL 40/4 Liste)

cutechess-cli.exe -repeat -games 2 -rounds 100 -tournament round-robin -pgnout elo-%date%-40-10.pgn -resign movecount=3 score=400 -draw movenumber=34 movecount=8 score=20 -concurrency 6 -openings file=2moves_v1.pgn format=pgn order=random plies=16 -engine name=RubiChess-Bitboard cmd=Rubichess\RubiChess-Bitboard.exe proto=uci -engine name=RubiChess-Board88 cmd=Rubichess\RubiChess-Board88.exe proto=uci -engine name=VICE cmd=Vice\Vice10-64.exe proto=uci -engine name=Cinnamon cmd=Cinnamon\cinnamon_2.0_x64-modern.exe proto=uci -engine name=GunBorg cmd=Gunborg\gunborg1.35_w64m.exe proto=uci -engine name=Gully cmd=Gully\gully2.exe proto=xboard -engine name=Sayuri cmd=Sayuri\sayuri.exe proto=uci -engine name=SnailChess cmd=Snailchess\SnailChess_4013.exe proto=xboard -engine name=Embla cmd=Embla\embla.exe proto=uci -engine name=Heracles cmd=Heracles\heracles.exe proto=uci -each restart=on tc=40/10.0 -cputimer -recover

cutechess-cli.exe -repeat -games 2 -rounds 100 -tournament round-robin -pgnout elo-%date%-10+01.pgn -resign movecount=3 score=400 -draw movenumber=34 movecount=8 score=20 -concurrency 6 -openings file=2moves_v1.pgn format=pgn order=random plies=16 -engine name=RubiChess-Bitboard cmd=Rubichess\RubiChess-Bitboard.exe proto=uci -engine name=RubiChess-Board88 cmd=Rubichess\RubiChess-Board88.exe proto=uci -engine name=VICE cmd=Vice\Vice10-64.exe proto=uci -engine name=Cinnamon cmd=Cinnamon\cinnamon_2.0_x64-modern.exe proto=uci -engine name=GunBorg cmd=Gunborg\gunborg1.35_w64m.exe proto=uci -engine name=Gully cmd=Gully\gully2.exe proto=xboard -engine name=Sayuri cmd=Sayuri\sayuri.exe proto=uci -engine name=SnailChess cmd=Snailchess\SnailChess_4013.exe proto=xboard -engine name=Embla cmd=Embla\embla.exe proto=uci -engine name=Heracles cmd=Heracles\heracles.exe proto=uci -each restart=on tc=10.0+0.1 -cputimer -recover

