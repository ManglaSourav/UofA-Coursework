package edu.arizona.cs;

import java.util.List;
import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.assertEquals;


public class TestQ5 {

    @Test
    public void testQ5_1() {
        String inputFileFullPath="src/main/resources/Docs.txt";
        try {
            String query = "schizophrenia AND drug";
            InvertedIndex objInvertedIndex = new InvertedIndex(inputFileFullPath);
            String[] ans_q5_1 = objInvertedIndex.runQ5_1(query);
            String[] doc_names_q1 = {"Doc1", "Doc4"};
            int counter1 = 0;
            assertEquals((doc_names_q1.length) , ans_q5_1.length);
            for (String docs : ans_q5_1) {
                assertEquals(doc_names_q1[counter1], ans_q5_1[counter1]);
                counter1 = counter1 + 1;
            }
        }
        catch ( java.io.FileNotFoundException ex)
        {
            System.out.println(ex.getMessage()); }

        catch ( java.io.IOException ex)
        {
            System.out.println(ex.getMessage()); }

    }


 @Test
    public void testQ5_2() {
        String inputFileFullPath="src/main/resources/Docs.txt";
        try {
            String query = "breakthrough OR new";
            InvertedIndex objInvertedIndex = new InvertedIndex(inputFileFullPath);
            String[] ans_q5_2 = objInvertedIndex.runQ5_2(query);
            String[] doc_names_q2 = {"Doc1", "Doc2", "Doc3", "Doc4"};
            int counter1 = 0;
            assertEquals((doc_names_q2.length) , ans_q5_2.length);
            for (String docs : ans_q5_2) {
                assertEquals(doc_names_q2[counter1], ans_q5_2[counter1]);
                counter1 = counter1 + 1;
            }
        }
        catch ( java.io.FileNotFoundException ex)
        {
            System.out.println(ex.getMessage()); }

        catch ( java.io.IOException ex)
        {
            System.out.println(ex.getMessage()); }

    }


     @Test
    public void testQ5_3() {
        String inputFileFullPath="src/main/resources/Docs.txt";
        try {
            String query = "(drug OR treatment) AND schizophrenia";
            InvertedIndex objInvertedIndex = new InvertedIndex(inputFileFullPath);
            String[] ans_q5_3 = objInvertedIndex.runQ5_3(query);
            String[] doc_names_q3 = {"Doc1", "Doc2", "Doc4"};
            int counter1 = 0;
            assertEquals((doc_names_q3.length) , ans_q5_3.length);
            for (String docs : ans_q5_3) {
                assertEquals(doc_names_q3[counter1], ans_q5_3[counter1]);
                counter1 = counter1 + 1;
            }
        }
        catch ( java.io.FileNotFoundException ex)
        {
            System.out.println(ex.getMessage()); }

        catch ( java.io.IOException ex)
        {
            System.out.println(ex.getMessage()); }

    }

}



