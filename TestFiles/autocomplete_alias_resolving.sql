SELECT emp.ename FROM emp;
/

SELECT e.ename FROM emp e;
/

SELECT e.ename , e.deptno , d.deptno, d.dname FROM scott.emp e, "SCOTT"."DEPT" d WHERE e.deptno = d.deptno;
/

SELECT
	e.empno	,
	e.ename	,
	d.dname	,
	(SELECT	d1.deptno FROM	dept	d1 WHERE d1.deptno	=	e.deptno) col,
		d.deptno
FROM emp e,
	dept d
WHERE	e.deptno = d.deptno
  AND e.sal > 0;

BEGIN
  SELECT
    e.empno ,
    e.ename ,
	(SELECT	d1.deptno FROM	dept	d1 WHERE d1.deptno	=	e.deptno) col
  FROM emp e
  INNER JOIN scott.dept d
    ON (e.deptno = d.deptno )
  WHERE e.hiredate > SYSDATE
  ;
END;
/

SELECT *
FROM emp e, (SELECT d1.* FROM dept d1) d
WHERE e.deptno = d.deptno
;
/

SELECT
  e.empno ,
  e.ename,
  d.deptno,
  (SELECT d0.dname FROM dept d0 WHERE d0.deptno  = e.deptno) col,
  NULL dummy
FROM emp e,
  (SELECT d1.* FROM dept d1) d
WHERE e.deptno = d.deptno
  AND EXISTS (SELECT 1 FROM emp e2 WHERE e2.deptno = e.deptno);
/

CREATE TABLE "emp_in_lower_case" AS SELECT * FROM emp;
/

SELECT
  e.empno ,
  e.ename,
  d.dname ,
  (SELECT e1.comm FROM "EMP" e1 WHERE e1.empno = e.empno) col,
  NULL dummy
FROM "emp_in_lower_case" e,
  (SELECT * FROM "DEPT" d1) d
WHERE e.deptno = d.deptno
  AND EXISTS (SELECT 1 FROM "EMP" e2 WHERE e2.deptno = e.deptno);
/

SELECT emp.empno FROM EMP;
/

SELECT *
  FROM (SELECT e1.* FROM emp e1) e
  INNER JOIN (SELECT d1.* FROM dept d1 WHERE d1.deptno > 0) d
    ON (e.deptno = d.deptno )
  WHERE e.hiredate > SYSDATE
  AND d.loc IS NOT NULL;
/

SELECT e.
  FROM (SELECT * FROM emp) e
  INNER JOIN (SELECT * FROM dept) d
    ON (e.deptno = d.deptno)
/

SELECT e.ename
  FROM emp e
  INNER JOIN dept d
    ON (e.deptno = d.deptno)
/

SELECT e.ename, d.dname, e.sal
FROM emp e
JOIN dept d ON e.deptno = d.deptno
WHERE e.sal > 1000
/

WITH e1 AS (SELECT * FROM emp)
SELECT e2.ename FROM e1 e2
/

WITH e1 AS (SELECT * FROM emp),
  e2 AS (SELECT * FROM e1),
  e3 AS (SELECT * FROM e2)
SELECT e.hiredate FROM e3 e
/

WITH e1 AS (SELECT * FROM emp),
  e2 AS (SELECT * FROM e1),
  e3 AS (SELECT * FROM e2)
SELECT e3.hiredate FROM e3
/


WITH e1 AS (SELECT e0.* FROM emp e0)
SELECT e2.empno , d.dname FROM dept d, e1 e2
;
/

WITH e1 AS (SELECT e0.* FROM emp e0),
 d1 AS (SELECT d0.* FROM dept d0)
SELECT e2.ename, d2.dname FROM e1 e2, d1 d2
  WHERE e2.deptno = d2.deptno;
/

WITH e1 AS (SELECT e0.empno FROM emp e0)
  SELECT e2. FROM e1 e2;

-- not working, as expected
WITH e1 AS (SELECT e2.empno, e2.job FROM emp e2),
  e3 AS (SELECT e4.* FROM e1 e4)
SELECT e5. FROM e3 e5;
/

-- it is good because of explicit column list on every level
WITH e1 AS (SELECT e2.empno, e2.job FROM emp e2),
  e3 AS (SELECT e4.empno, e4.job FROM e1 e4)
SELECT e5.empno, e5.job FROM e3 e5;
/

WITH e1 (employee_num, job_name) AS (SELECT e2.empno col11, e2.job col12 FROM emp e2)
SELECT e3.employee_num FROM e1 e3;
/

WITH e1 (employee_num, job_name) AS (SELECT e2.empno col11, e2.job col12 FROM emp e2),
  e3 (employee_number, job_name) AS (SELECT e4.employee_num col21, e4.job_name col22 FROM e1 e4)
SELECT e5.employee_number FROM e3 e5;
/

-- d2 deptno is not resolved, expected
WITH e1 AS (SELECT e0.* FROM emp e0),
 d1 AS (SELECT d0.deptno, dname AS department_name, d0.loc location FROM dept d0),
 d11 AS (SELECT d01.* FROM d1 d01, e1 e01 where d01.deptno = e01.deptno)
SELECT e2.*, d2.* FROM e1 e2, d11 d2
  WHERE e2.deptno = d2.deptno ;
/

WITH e0 AS (SELECT ename, job, hiredate FROM emp)
SELECT * FROM emp e1 WHERE e1.hiredate < SYSDATE
UNION
SELECT * FROM emp e2 WHERE e2.hiredate < SYSDATE
;

WITH e0 (ename, job, hiredate) AS (SELECT ename, job, hiredate FROM emp)
SELECT * FROM e0 e1 WHERE e1.hiredate < SYSDATE
UNION
SELECT * FROM e0 e2 WHERE e2.hiredate < SYSDATE
;