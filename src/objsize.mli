(** Compute memory usage of OCaml heap values (by recursively inspecting runtime representation) *)


type tag_info = {
  tag : int; (** heap value tag *)
  count : int; (** number of values with this tag (eq. words used for headers) *)
  words : int; (** heap words used for data *)
}

(** Information gathered while walking through values. *)
type info =
  { data : int
  ; headers : int
  ; depth : int
  ; tags : tag_info array; (** counts grouped by tags @since 0.17 *)
  }

(** Returns information for first argument. *)
val objsize : 'a -> info

(** [objsize_limit limit v] 
    @return memory usage information for [v].
    [limit] limits the maximum depth to follow (to prevent excess
    stack usage). If maximum depth is reached during heap traversal then the
    returned result may be less than actual. One can detect this
    situation by comparing returned depth against the limit.
    @since 0.17 *)
val objsize_limit : int -> 'a -> info

(** Calculate size in bytes *)
val size_with_headers : info -> int
val size_without_headers : info -> int

(** Walk through all the roots and return information for the whole heap.
    @since 0.17 *)
val objsize_roots : unit -> info

(** Walk through all the roots and return information for the whole heap.
    Integer parameter limits the maximum depth to follow (to prevent excess
    stack usage). If maximum depth is reached during heap traversal then the
    returned result may be less than actual. One can detect this
    situation by comparing returned depth against the limit.
    @since 0.17 *)
val objsize_roots_limit : int -> info

(** [sub next base] calculate [next - base], i.e. the changes from [base] to [next].
    @param tweak account for the memory occupied by one [info] structure, default true
    @since 0.17 *)
val sub : ?tweak:bool -> info -> info -> info

(** @return human readable name for the tag number
    @since 0.17 *)
val string_of_tag : int -> string

(** @return human readable representation of memory usage in general
    @param map convert number of words to string
    @since 0.17
*)
val show_info : ?map:(int -> string) -> info -> string

(** @return human readable representation of memory usage grouped by tags
    @param map convert number of words to string
    @since 0.17
*)
val show_tags : ?map:(int -> string) -> info -> string list
