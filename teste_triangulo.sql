-- Teste do triangulo azul de execucao: rode com "Execute script" (F5) e observe a margem.
-- Cada bloco dura ~3 segundos, entao da para ver o triangulo andando de um para o outro.

BEGIN
  Dbms_Output.Put_Line('Bloco 1');
  Dbms_Lock.Sleep(3);
END;
/

BEGIN
  Dbms_Output.Put_Line('Bloco 2');
  Dbms_Lock.Sleep(3);
END;
/

BEGIN
  Dbms_Output.Put_Line('Bloco 3');
  Dbms_Lock.Sleep(3);
END;
/
