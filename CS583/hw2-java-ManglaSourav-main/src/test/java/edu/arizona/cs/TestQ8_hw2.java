package edu.arizona.cs;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.assertEquals;


public class TestQ8_hw2 {

    @Test
    public void testQ8_1_1() {
        String inputFileFullPath="src/main/resources/Docs.txt";
        try {
            String query = "schizophrenia /2 drug";
            Document ans1=new Document("Doc1",3,1);
            Document ans2=new Document("Doc2",1,2);
            Document[] gold_q8_1_1= {ans1,ans2};
            InvertedIndex objInvertedIndex = new InvertedIndex(inputFileFullPath);
            Document[] ans_q8_1_1 = objInvertedIndex.runQ8_1_1(query);

            assert ans_q8_1_1 !=null;
            assert ans_q8_1_1.length > 0;
            assert ans_q8_1_1.length == 2;
            int counter=0;
            for(Document doc: gold_q8_1_1) {
               assert doc.equals(ans_q8_1_1[counter])==true;
                counter+=1;
            }
        }
        catch ( java.io.FileNotFoundException ex)
        {
            System.out.println(ex.getMessage()); }
        catch (ArrayIndexOutOfBoundsException ex) {
            System.out.println(ex.getMessage());
        }
    }

    @Test
    public void testQ8_1_2() {
        String inputFileFullPath="src/main/resources/Docs.txt";
        try {
            String query = "schizophrenia /4 drug";
            Document ans1=new Document("Doc1",3,1);
            Document ans2=new Document("Doc2",1,2);
            Document ans3=new Document("Doc3",5,1);
            Document[] gold_q8_1_2= {ans1,ans2,ans3};
            InvertedIndex objInvertedIndex = new InvertedIndex(inputFileFullPath);
            Document[] ans_q8_1_2 = objInvertedIndex.runQ8_1_2(query);

            assert ans_q8_1_2 !=null;
            assert ans_q8_1_2.length > 0;
            assert ans_q8_1_2.length == 3;
            int counter=0;
            for(Document doc: gold_q8_1_2) {
                assert doc.equals(ans_q8_1_2[counter])==true;

                counter+=1;
            }
        }
        catch ( java.io.FileNotFoundException ex)
        {
            System.out.println(ex.getMessage()); }
        catch (ArrayIndexOutOfBoundsException ex) {
            System.out.println(ex.getMessage());
        }
    }

    @Test
    public void testQ8_2_directional() {
        String inputFileFullPath="src/main/resources/Docs.txt";
        try {
            String query = "schizophrenia /4 drug";
            Document ans1=new Document("Doc2",1,2);
            Document[] gold_q8_1_2= {ans1};
            InvertedIndex objInvertedIndex = new InvertedIndex(inputFileFullPath);
            Document[] ans_q8_2_directional = objInvertedIndex.runQ8_2_directional(query);

            assert ans_q8_2_directional !=null;
            assert ans_q8_2_directional.length > 0;
            assert ans_q8_2_directional.length == 1;
            int counter=0;
            for(Document doc: gold_q8_1_2) {
                assert doc.equals(ans_q8_2_directional[counter])==true;
                counter+=1;
            }
        }
        catch ( java.io.FileNotFoundException ex)
        {
            System.out.println(ex.getMessage()); }
        catch (ArrayIndexOutOfBoundsException ex) {
            System.out.println(ex.getMessage());
        }
    }


}



