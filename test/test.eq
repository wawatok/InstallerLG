(= 1 1) ; "","1",""
(= 0 1) ; "","0",""
(= -0 -0) ; "","1",""
(= "1" "1") ; "","1",""
(= "0" "1") ; "","0",""
(= 1 "1") ; "","1",""
(= 0 "1") ; "","0",""
(= "1" 1) ; "","1",""
(= "0" 1) ; "","0",""
(= 0 "") ; "","1",""
(= -0 "") ; "","1",""
(= "" "") ; "","1",""
(= "abc" "abc") ; "","1",""
(= "abc" "ab") ; "","0",""
(set a "yes") (= "yes" a); "","1",""
(set a "yes") (= "no" a); "","0",""
(set a "yes") (= a "yes"); "","1",""
(set a "yes") (= a "no"); "","0",""
