(method (public static) "LSwitch;.branch:(ILjava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;"
 (
  (load-param v0)
  (load-param-object v1)
  (load-param-object v2)
  (if-eqz v0 :else)
  (return-object v1)
  (:else)
  (return-object v2)
 )
)
