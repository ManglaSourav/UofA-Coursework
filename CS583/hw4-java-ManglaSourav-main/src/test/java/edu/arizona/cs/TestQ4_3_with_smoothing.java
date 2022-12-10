package edu.arizona.cs;
import java.util.List;
import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.hamcrest.MatcherAssert.assertThat;
import edu.arizona.cs.Document;

public class TestQ4_3_with_smoothing {

    @Test
    public void testDocsAndScores() {
        String inputFileFullPath="input.txt";
        QueryEngineQ4 objQueryEngine = new QueryEngineQ4(inputFileFullPath);
        try {
            String[] common_query = {"information", "retrieval"};
            List<ResultClass> ans = objQueryEngine.runQ4_3_with_smoothing(common_query);
            String[] doc_names_q1 = {"Doc1", "Doc2",  "Doc4","Doc3"};
            assertEquals(ans.size(), (doc_names_q1.length));
            assertEquals(doc_names_q1[0], ans.get(0).DocName.docid);
            assertEquals(doc_names_q1[1], ans.get(1).DocName.docid);
            assertThat( "ans", ans.get(0).docScore > ans.get(1).docScore);
            assertThat( "ans", ans.get(1).docScore > ans.get(2).docScore);
            assertThat( "ans", ans.get(1).docScore > ans.get(3).docScore);
        }
        catch (Exception ex) {
            System.out.println(ex.getMessage());
        }
    }


}



