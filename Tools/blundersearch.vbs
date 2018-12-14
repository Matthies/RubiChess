
pgnfilename = "wbm7.pgn"

Const ForReading = 1, ForWriting = 2, ForAppending = 8

pgnfilename = InputBox("PGN-file to analyse", "pgn2time-avg", pgnfilename)

Set fso = CreateObject("Scripting.FileSystemObject")
Set pgnfile = fso.OpenTextFile(pgnfilename , ForReading)

dim Depthsum(2)
dim count(2)
dim Names(2)
dim color(2)
dim ttm(2)
dim lastscore(2)
dim thisscore(2)

set regHeader = new Regexp
regHeader.pattern = "\[.*\]"
regHeader.IgnoreCase = true
set regWhite = new Regexp
regWhite.pattern = "\[White\s+\""(.*)\"".*\]"
regWhite.IgnoreCase = true
set regBlack = new Regexp
regBlack.pattern = "\[Black\s+\""(.*)\"".*\]"
regBlack.IgnoreCase = true
set regMove = new Regexp
regMove.pattern = "((\d+)\.\s+)?([a-hKQRBNxOo0-8\#\=\-\+]{2,})(\s+|$)(\{((\S*)/(\d+)\s+)?(\d+(\.\d+)?)s[^\}]*\})?"
regMove.IgnoreCase = true
regMove.global = true
games = 0

while not pgnfile.AtEndOfStream
  l = pgnfile.ReadLine
  if regWhite.Test(l) then
    White = regWhite.Replace(l, "$1")
    if Names(0) = "" then
      Names(0) = White
    end if
    games = games + 1
  end if
  if regBlack.Test(l) then
    Black = regBlack.Replace(l, "$1")
    if Names(1) = "" then
      Names(1) = Black
    end if
    nexttomove = 0
    nextcol = 0
    thisscore(0) = 0
    thisscore(1) = 0
    if White = Names(0) and Black = Names(1) then
      color(0) = 0
      color(1) = 1
    elseif White = Names(1) and Black = Names(0) then
      color(0) = 1
      color(1) = 0
    else
      wscript.echo "Wer ist weiﬂ, wer ist schwarz??"
    end if
  end if

  isHeader = regHeader.Test(l)
  if not isHeader and regMove.Test(l) then
    set matches = regMove.Execute(l)
    for each m in matches
      set sm = m.SubMatches
      if sm(1) <> "" then
        col = 0
        nexttomove = nexttomove + 1
        if nexttomove <> Cint(sm(1)) then
          wscript.echo "Alarm: " & nexttomove & " <> " & sm(1)
          wscript.quit
        end if
      else
        col = 1
      end if
      if sm(6) <> empty then
        if instr(sm(6), "+M") then
          score = 320.0 - 0.01 * CDbl(mid(sm(6),3))
        elseif instr(sm(6), "-M") then
          score = -320.0 + 0.01 * CDbl(mid(sm(6),3))
        else
          score = CDbl(Replace(sm(6), ".", ","))
        end if
        ' wscript.echo score
      else
        'wscript.echo "empty"
        'wscript.echo l & " " & col & "/" & nextcol
      end if
      if col <> nextcol or sm(7) = empty then
        'wscript.echo "Alarm: " & nexttomove & "/" & nextcol & "/" & col & " " & l
      else
        ' wscript.echo nexttomove & " " & names(color(col)) & " " & depth
        c = color(col)
        o = 1 - c
        lastscore(c) = thisscore(c)
        thisscore(c) = score
        s1 = lastscore(c)
        s2 = score
        ' Blunder, wenn alle der folgenden Kriterien erf¸llt sind:
        ' a. score f‰llt um mindestens 7 Punkte
        ' b. score war vorher noch nicht schlechter als -3 Punkte
        ' c. score ist jetzt schlechter als 10 Punkte
        if (s1 - s2 > 7) and (s2 > -3) and (s2 < 10) then
          wscript.echo vbcr
          wscript.echo "Game: " & games
          wscript.echo "Move: " & nexttomove
          wscript.echo "Engine: " & Names(c)
          wscript.echo "Score: " & s1 & " -> " & s2
        end if
      end if
      
      nextcol = 1 - nextcol
    next
  end if 

wend
