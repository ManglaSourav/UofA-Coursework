package edu.arizona.cs;
import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.hamcrest.MatcherAssert.assertThat;


public class TestQ5_2_f1score {
    @Test
    public void testDocsAndScores() {

        QueryEngineQ5 objQueryEngine = new QueryEngineQ5();
        try {
            double ans = objQueryEngine.runQ5_2_f1score();
            assertThat("ans",ans>0.8);
        } catch (Exception ex) {
            System.out.println(ex.getMessage());
        }
    }
}



