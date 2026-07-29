show databases;

use LHY_DB;

SHOW tableS;

select * from TBL_USER;

insert into TBL_USER(U_NAME, U_GENGDER) VALUES('lhy', 'man');


delimiter ##
CREATE procedure PROC_DELETE_USER(IN UNAME VARCHAR(32))
BEGIN
SET SQL_SAFE_UPDATES=0;
delete from TBL_USER where U_NAME=UNAME;
SET SQL_SAFE_UPDATES=1;
END##

CALL PROC_DELETE_USER('LHY')