
' pgnfilename = "elo-02.11.2017-10+01.pgn"
pgnfilename = "elo-01.11.2017-40-10.pgn"
pgnfilename = "elo-04.11.2017-10+01.pgn"

Const ForReading = 1, ForWriting = 2, ForAppending = 8

Set fso = CreateObject("Scripting.FileSystemObject")
Set pgnfile = fso.OpenTextFile(pgnfilename , ForReading)
set csvdepthfile = fso.OpenTextFile(pgnfilename & "-depth.csv" , ForWriting, true)
set csvtimefile = fso.OpenTextFile(pgnfilename & "-time.csv" , ForWriting, true)

dim Times(2)
dim Depth(2)
dim Names(2)

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
regMove.pattern = "((\d+)\.\s+)?([a-hKQRBNxOo0-8\#\=\-\+]{2,})(\s+|$)(\{(\S*/(\d+)\s+)?(\d+\.\d+)s[^\}]*\})?"
regMove.IgnoreCase = true
regMove.global = true
games = 0

while not pgnfile.AtEndOfStream
  l = pgnfile.ReadLine
  if regWhite.Test(l) then
    Names(0) = regWhite.Replace(l, "$1")
    games = games + 1
    if games > 1 then
      csvdepthfile.WriteLine Depth(0)
      csvtimefile.writeline Times(0)
    end if
    Times(0) = Names(0) & ";"
    Depth(0) = Names(0) & ";"
  end if
  if regBlack.Test(l) then
    Names(1) = regBlack.Replace(l, "$1")
    if games > 1 then
      csvdepthfile.WriteLine Depth(1)
      csvtimefile.writeline Times(1)
    end if
    Times(1) = Names(1) & ";"
    Depth(1) = Names(1) & ";"
    nexttomove = 0
    wscript.echo "Game " & games & ": " & Names(0) & " - " & Names(1)
  end if

  isHeader = regHeader.Test(l)
  if not isHeader and regMove.Test(l) then
    set matches = regMove.Execute(l)
    for each m in matches
    ' wscript.echo m
    for each ss in m.SubMatches
      'wscript.echo ss
    next
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
      Depth(col) = Depth(col) & sm(6) & ";"
      Times(col) = Replace(Times(col), ".", ",") & sm(7) & ";"
    next
  end if 

wend
