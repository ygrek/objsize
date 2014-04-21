open Printf
;

value print title val =
  let i = Objsize.objsize val in
  printf "%S : data_words=%i headers=%i depth=%i\n    \
bytes_without_headers=%i bytes_with_headers=%i\n%!"
    title i.Objsize.data i.Objsize.headers i.Objsize.depth
    (Objsize.size_without_headers i)
    (Objsize.size_with_headers i)
;

print "string of 13 chars" ("0123456" ^ "789012")
;

print "some object"
  ( object method x = 123; method y = print_int; end
  )
;

print "some float" (Random.float 1.)
;

value rec cyc = [1 :: [2 :: [3 :: cyc]]]
;

print "cyclic list" cyc
;

value genlist n =
  let rec inner acc n =
    if n <= 0
    then acc
    else inner [n :: acc] (n-1)
  in
    inner [] n
;

print "big list" (genlist 30000)
;

print "big array" (Array.make 30000 True)
;

print "statically created value" [1; 2; 3]
;
