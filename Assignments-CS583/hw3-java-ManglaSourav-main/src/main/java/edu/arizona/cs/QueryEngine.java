package edu.arizona.cs;

import org.apache.lucene.analysis.standard.StandardAnalyzer;
import org.apache.lucene.document.Document;
import org.apache.lucene.document.Field;
import org.apache.lucene.document.StringField;
import org.apache.lucene.document.TextField;
import org.apache.lucene.index.DirectoryReader;
import org.apache.lucene.index.IndexReader;
import org.apache.lucene.index.IndexWriter;
import org.apache.lucene.index.IndexWriterConfig;
import org.apache.lucene.queryparser.classic.ParseException;
import org.apache.lucene.queryparser.classic.QueryParser;
import org.apache.lucene.search.IndexSearcher;
import org.apache.lucene.search.Query;
import org.apache.lucene.search.ScoreDoc;
import org.apache.lucene.search.TopDocs;
import org.apache.lucene.search.similarities.*;
import org.apache.lucene.store.Directory;
import org.apache.lucene.store.RAMDirectory;
import org.apache.lucene.util.BytesRef;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.*;


class MySimilarity extends TFIDFSimilarity {
    @Override
    public float tf(float freq) {
        return (float) Math.sqrt(freq);
    }

    @Override
    public float idf(long docFreq, long numDocs) {
        return (float) (Math.log(numDocs / (docFreq + 1)) + 1);
    }

    @Override
    public float lengthNorm(int numTerms) {
        return (float) (1 / Math.sqrt(numTerms));
    }

    @Override
    public float sloppyFreq(int distance) {
        return 1 / ((float) distance + 1);
    }

    @Override
    public float scorePayload(int doc, int start, int end, BytesRef payload) {
        return 1;
    }
}


public class QueryEngine {
    boolean indexExists = false;
    String inputFilePath = "";
    StandardAnalyzer analyzer = new StandardAnalyzer();
    Directory index = new RAMDirectory();

    public QueryEngine(String inputFile) {
        try {

            inputFilePath = "./src/main/resources/" + inputFile;
            buildIndex(false);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void buildIndex(boolean changeSimilarity) throws IOException {
        try {
            //Get file from resources folder
            IndexWriterConfig config = new IndexWriterConfig(analyzer);
            if (changeSimilarity) {
                config.setSimilarity(new MySimilarity());
            }
            IndexWriter w = new IndexWriter(index, config);
            if (changeSimilarity) {
//                delete previous indexed document because we have changed similarity and we have to index our document again with new similarity.
                w.deleteAll();
            }
            BufferedReader br = new BufferedReader(new FileReader(inputFilePath));
            String docs;
            while ((docs = br.readLine()) != null) {
                System.out.println(docs);
                String docId = docs.substring(0, 4).trim();
                String docData = docs.substring(4, docs.length()).trim();
                addDoc(w, docData, docId);
            }
            w.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
        indexExists = true;
    }

    private static void addDoc(IndexWriter w, String docData, String docId) throws IOException {
        Document doc = new Document();
        doc.add(new TextField("title", docData, Field.Store.YES));
        doc.add(new StringField("docid", docId, Field.Store.YES));
        w.addDocument(doc);
    }

    public static void main(String[] args) {
        try {
            String fileName = "input.txt";
            System.out.println("********Welcome to  Homework 3!");
            String[] query13a = {"information", "retrieval"};
            QueryEngine objQueryEngine = new QueryEngine(fileName);
            List<ResultClass> result = objQueryEngine.runQ1_3(query13a);
        } catch (Exception ex) {
            System.out.println(ex.getMessage());
        }
    }

    public List<ResultClass> commandQueryParser(String combinedQuery, boolean changeSimilarity) throws java.io.FileNotFoundException, java.io.IOException {
        try {
            Query q = new QueryParser("title", analyzer).parse(combinedQuery);
            int hitsPerPage = 10;
            IndexReader reader = DirectoryReader.open(index);
            IndexSearcher searcher = new IndexSearcher(reader);
            if (changeSimilarity) {
                searcher.setSimilarity(new MySimilarity());
            }
            TopDocs docs = searcher.search(q, hitsPerPage);
            ScoreDoc[] hits = docs.scoreDocs;
            List<ResultClass> doc_score_list = new ArrayList<ResultClass>();
            for (ScoreDoc hit : hits) {
                int docId = hit.doc;
                System.out.println("hit" + hit);
                Document d = searcher.doc(docId);

                Document foundDoc = new Document();
                foundDoc.add(new TextField("title", d.get("title"), Field.Store.YES));
                foundDoc.add(new StringField("docid", d.get("docid"), Field.Store.YES));
                ResultClass objResultClass = new ResultClass();
                objResultClass.DocName = foundDoc;
                objResultClass.docScore = hit.score;
                doc_score_list.add(objResultClass);
            }
            return doc_score_list;
        } catch (ParseException e) {
            e.printStackTrace();
            System.out.println(e.getMessage());
        }
        return new ArrayList<ResultClass>();
    }

    public List<ResultClass> runQ1_1(String[] query) throws java.io.FileNotFoundException, java.io.IOException {
        if (!indexExists) {
            buildIndex(false);
        }
        String combinedQuery = String.join(" ", query);
        return commandQueryParser(combinedQuery, false);
    }

    public List<ResultClass> runQ1_2_a(String[] query) throws java.io.FileNotFoundException, java.io.IOException {
        if (!indexExists) {
            buildIndex(false);
        }
        String combinedQuery = String.join(" AND ", query);
        return commandQueryParser(combinedQuery, false);
    }

    public List<ResultClass> runQ1_2_b(String[] query) throws java.io.FileNotFoundException, java.io.IOException {
        if (!indexExists) {
            buildIndex(false);
        }
        String combinedQuery = String.join(" AND NOT ", query);
        return commandQueryParser(combinedQuery, false);
    }

    public List<ResultClass> runQ1_2_c(String[] query) throws java.io.FileNotFoundException, java.io.IOException {
        if (!indexExists) {
            buildIndex(false);
        }
        String combinedQuery = "\"" + String.join(" AND ", query) + "\"~1";
        System.out.println(combinedQuery);
        return commandQueryParser(combinedQuery, false);
    }

    public List<ResultClass> runQ1_3(String[] query) throws java.io.FileNotFoundException, java.io.IOException {


        buildIndex(true);

        String combinedQuery = String.join(" ", query);
        System.out.println(combinedQuery);
//        after changing similarity there is no change in ordering of the documents
        return commandQueryParser(combinedQuery, true);
    }

}
