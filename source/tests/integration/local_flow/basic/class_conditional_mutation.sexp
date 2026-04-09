(method (public) "LMyObj;.eitherAOrArg:(ILjava/lang/Object;)Ljava/lang/Object;"
 (
  (load-param-object v0)
  (load-param v1)
  (load-param-object v2)
  (if-eqz v1 :skip)
  (iput-object v2 v0 "LMyObj;.a:Ljava/lang/Object;")
  (:skip)
  (iget-object v0 "LMyObj;.a:Ljava/lang/Object;")
  (move-result-pseudo-object v3)
  (return-object v3)
 )
)
