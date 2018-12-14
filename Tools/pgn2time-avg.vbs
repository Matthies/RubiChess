
pgnfilename = "ks33-2.pgn"

Const ForReading = 1, ForWriting = 2, ForAppending = 8

pgnfilename = InputBox("PGN-file to analyse", "pgn2time-avg", pgnfilename)

Set fso = CreateObject("Scripting.FileSystemObject")
Set pgnfile = fso.OpenTextFile(pgnfilename , ForReading)

dim Depthsum(2)
dim count(2)
dim Names(2)
dim color(2)
dim ttm(2)
dim lastdepth(2)
dim thisdepth(2)
dim betterdepth(2)

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
    thisdepth(0) = 0
    thisdepth(1) = 0
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
      if sm(7) <> empty then
        tm = CDbl(Replace(sm(7), ".", ","))
      else
        'wscript.echo "empty"
        'wscript.echo l & " " & col & "/" & nextcol
      end if
      depth = CInt(sm(6))
      if col <> nextcol or sm(7) = empty then
        ' wscript.echo "Alarm: " & nexttomove & "/" & nextcol & "/" & col & " " & l
      elseif depth = 444 then
        ' wscript.echo depth
      else
        ' wscript.echo nexttomove & " " & names(color(col)) & " " & depth
        c = color(col)
        o = 1 - c
        lastdepth(c) = thisdepth(c)
        thisdepth(c) = depth
        od = thisdepth(o)
        cd1 = lastdepth(c)
        cd2 = depth
        if od > 0 and cd1 > 0 then
          if (od > cd1 and od >= cd2) or (od > cd2 and od >= cd1) then
            betterdepth(o) = betterdepth(o) + 1
          end if
        end if 
        Depthsum(c) = Depthsum(c) + depth
        ttm(c) = ttm(c) + tm
        count(c) = count(c) + 1
      end if
      
      nextcol = 1 - nextcol
    next
  end if 

wend
wscript.echo pgnfilename
wscript.echo "Depth:"
wscript.echo names(0) & ": " & depthsum(0) & " / " & count(0) & " = "  & depthsum(0) / count(0)
wscript.echo names(1) & ": " & depthsum(1) & " / " & count(1) & " = "  & depthsum(1) / count(1)
wscript.echo "Movetime:"
wscript.echo names(0) & ": " & ttm(0) & " / " & count(0) & " = "  & ttm(0) / count(0)
wscript.echo names(1) & ": " & ttm(1) & " / " & count(1) & " = "  & ttm(1) / count(1)
if betterdepth(0) > 0 and betterdepth(1) > 0 then
  wscript.echo "Betterdepth:"
  wscript.echo names(0) & "/" & names(1) & " = " & betterdepth(0) & "/" & betterdepth(1) & " = " & betterdepth(0)/betterdepth(1)
end if
