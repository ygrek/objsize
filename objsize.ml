
if Sys.ocaml_version < "3.11"
then
  failwith "Objsize >=0.12 can only be used with OCaml >=3.11"

open Printf

(* TODO group custom by name ? *)

type tag_info = { tag : int; count : int; words : int; }

type info =
  { data : int
  ; headers : int
  ; depth : int
  ; tags : tag_info array;
  }

external internal_objsize : int -> Obj.t -> info = "ml_objsize"

let objsize_limit limit obj = internal_objsize limit (Obj.repr obj)
let objsize obj = objsize_limit max_int obj

let size_with_headers i = (Sys.word_size/8) * (i.data + i.headers)

let size_without_headers i = (Sys.word_size/8) * i.data

external objsize_roots_limit : int -> info = "ml_objsize_roots"

let objsize_roots () = objsize_roots_limit max_int

let sub ?(tweak=true) a b =
  let rec merge acc l1 l2 =
    match l1, l2 with
    | l, [] | [], l -> List.rev_append acc l
    | a::aa, b::bb when a.tag = b.tag ->
      if a.count = b.count then (* zero *)
        merge acc aa bb
      else
        let v = { tag = a.tag; count = a.count - b.count; words = a.words - b.words; } in
        merge (v::acc) aa bb
    | a::aa, b::bb ->
      if a.tag < b.tag then
        merge (a::acc) aa l2
      else (* b.tag < a.tag *)
        merge (b::acc) l1 bb
  in
  let a = if tweak then
    let size = Array.length a.tags in
    { a with
      data = a.data - 4 - size * 4;
      headers = a.headers - 2 - size;
      tags = (let a = Array.copy a.tags in
        a.(0) <- { a.(0) with
          count = a.(0).count - 2 - size;
          words = a.(0).words - 4 - size * 4;
        };
        a);
    }
    else a
  in
  { data = a.data - b.data;
    headers = a.headers - b.headers;
    depth = 0; (* ? *)
    tags = Array.of_list (merge [] (Array.to_list a.tags) (Array.to_list b.tags)); }

let string_of_tag n =
  if n > 255 || n < 0 then invalid_arg "string_of_tag";
  match n with
  | 246 -> "lazy"
  | 247 -> "closure"
  | 248 -> "object"
  | 249 -> "infix"
  | 250 -> "forward"
  | 251 -> "abstract"
  | 252 -> "string"
  | 253 -> "float"
  | 254 -> "[|float|]"
  | 255 -> "custom"
  | _ -> sprintf "Tag%d" n

let show_info ?(map=(sprintf "%d words")) i =
  sprintf "values %d, total %s, data %s, max depth %d"
    i.headers (map (i.data+i.headers)) (map i.data) i.depth

let show_tags ?(map=(sprintf "%d words")) i =
  List.map
    (fun ti -> sprintf "%9s : %d, total %s, data %s" (string_of_tag ti.tag) ti.count (map (ti.count+ti.words)) (map ti.words)) 
    (Array.to_list i.tags)
